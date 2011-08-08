#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <winsock.h>

#include "callout.h"
#include "btcodec.h"
#include "slotwait.h"
#include "slotsock.h"
#include "kad_route.h"
#include "recursive.h"
#include "kad_proto.h"
#include "udp_daemon.h"

#define SEARCH_TIMER 5000

static struct recursive_context *_search_slot = 0;

static void knode_copy(struct kad_node *dp, const struct kad_node *src)
{
	dp->kn_type = src->kn_type;
	dp->kn_addr = src->kn_addr;
	memcpy(dp->kn_ident, src->kn_ident, IDENT_LEN);
}

static int kad_bound_check(struct recursive_context *rcp, const struct kad_node *knp)
{
	int i;
	int count = 0;
	kad_node *rwp;

	for (i = 0; i < 8; i++) {
		if (rcp->rc_well_nodes[i].kn_type == 0) {
			count++;
			continue;
		}

		rwp = &rcp->rc_well_nodes[i];
		if (!memcmp(rwp->kn_ident, knp->kn_ident, IDENT_LEN)) {
			return 0;
		}

		if (kad_less_than(rcp->rc_target, knp->kn_ident, rwp->kn_ident)) {
			count++;
		}
	}

	return (count > 0);
}

static void kad_node_perf(const char *ident,
		struct recursive_node *rnp, struct recursive_node *perf[], size_t count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (perf[i] == NULL) {
			perf[i] = rnp;
			return;
		}
	}

	for (i = 0; i < count; i++) {
		if (rnp->rn_nout < perf[i]->rn_nout ||
				(rnp->rn_nout == perf[i]->rn_nout &&
				 kad_less_than(ident, rnp->kn_ident, perf[i]->kn_ident))) {
			perf[i] = rnp;
			return;
		}
	}
}

static void kad_recursive_output(void *upp)
{
	int i, j;
	int error = -1;
	int pending = 0;
	DWORD curtick = 0;
	struct sockaddr_in so_addr;
	struct recursive_node *rnp;
	struct recursive_node *perf[8];
	struct recursive_context *rcp;

	rcp = (struct recursive_context *)upp;

	assert(MAX_SEND_OUT < 8);
	for (i = 0; i < MAX_SEND_OUT; i++)
		perf[i] = (struct recursive_node *)0;

	curtick = GetTickCount();
	for (i = 0; i < MAX_PEER_COUNT; i++) {
		rnp = &rcp->rc_nodes[i];
		if (rnp->rn_type >= 1) {
			if (rnp->rn_access + SEARCH_TIMER > curtick) {
				error = 0, pending += (rnp->rn_type == 1);
			} else if (!kad_bound_check(rcp, rnp)) {
				continue;
			} else if (rnp->rn_nout < 3 && rnp->rn_type == 1) {
				kad_node_perf(rnp->kn_ident, rnp, perf, MAX_SEND_OUT);
			}
		}
	}

	for (i = 0; i + pending < MAX_SEND_OUT; i++) {
		rnp = perf[i];
		if (perf[i] != NULL) {
			so_addr.sin_family = AF_INET;
			so_addr.sin_port   = rnp->kn_addr.kc_port;
			so_addr.sin_addr   = rnp->kn_addr.kc_addr;
			assert(rnp->rn_type == 1);
			assert(rnp->rn_access + SEARCH_TIMER <= curtick);
			error = kad_proto_out(rcp->rc_tid, rcp->rc_type, rcp->rc_target, &so_addr);

			if (error == -1) {
				printf("kad_proto_out send error: %d\n", WSAGetLastError());
				break;
			}

			if (error == 1)
				return;

			kad_node_timed(rnp);
			rcp->rc_touch = curtick;
			rcp->rc_sentout++;
			rnp->rn_access = curtick;
			rnp->rn_nout++;
			error = 0;
		}
	}

	callout_reset(&rcp->rc_timeout, 1000);

	if (error == -1) {
		fprintf(stderr, "krpc end: tid %d, total %d, send %d, ack %d\n",
				rcp->rc_tid, rcp->rc_total, rcp->rc_sentout, rcp->rc_acked);
		if (rcp->rc_next != NULL)
			rcp->rc_next->rc_prev = rcp->rc_prev;
		*rcp->rc_prev = rcp->rc_next;
		waitcb_clean(&rcp->rc_timeout);
		delete rcp;
	}

	return;
}

static void kad_bound_update(struct recursive_context *rcp, const char *node)
{
	int i;
	int index = -1;
	kad_node *rwp;
	char bad_ident[20];

	for (i = 0; i < 8; i++) {
		rwp = &rcp->rc_well_nodes[i];
		if (rwp->kn_type == 1 && 
				!memcmp(rwp->kn_ident, node, IDENT_LEN))
			return;
	}

	for (i = 0; i < 8; i++) {
		rwp = &rcp->rc_well_nodes[i];
		if (rwp->kn_type == 0) {
			memcpy(rwp->kn_ident, node, IDENT_LEN);
			rwp->kn_type = 1;
			return;
		}
	}

	memcpy(bad_ident, node, 20);
	for (i = 0; i < 8; i++) {
		rwp = &rcp->rc_well_nodes[i];
		if (kad_less_than(rcp->rc_target,
					bad_ident, rwp->kn_ident)) {
			memcpy(bad_ident, rwp->kn_ident, 20);
			index = i;
		}
	}

	if (index != -1) {
		rwp = &rcp->rc_well_nodes[index];
		memcpy(rwp->kn_ident, node, 20);
		rwp->kn_type = 1;
		return;
	}
}

static void kad_recursive_update(struct recursive_context *rcp, struct kad_node *knp)
{
	int i;
	int index;
	int less_count = 0;
	char bad_ident[20];
	struct recursive_node *rnp;

	if (!kad_bound_check(rcp, knp)) {
		/* this node is bound out. */
		return;
	}

	index = -1;
	for (i = 0; i < MAX_PEER_COUNT; i++) {
		rnp = &rcp->rc_nodes[i];
		if (rnp->rn_type == 0) {
			index = i;
			continue;
		}

		if (!memcmp(rnp->kn_ident, knp->kn_ident, IDENT_LEN))
			return;
	}

	if (index != -1) {
		rnp = &rcp->rc_nodes[index];
		rcp->rc_total++;
		knode_copy(rnp, knp);
		rnp->rn_access = 0;
		rnp->rn_type = 1;
		rnp->rn_nout = 0;
		return;
	}

	for (i = 0; i < MAX_PEER_COUNT; i++) {
		rnp = &rcp->rc_nodes[i];
		if ((rnp->rn_nout == 1 && rnp->rn_type == 2) ||
				(rnp->rn_type == 2 && rnp->rn_access + SEARCH_TIMER < GetTickCount())) {
			rcp->rc_total++;
			knode_copy(rnp, knp);
			rnp->rn_type = 1;
			rnp->rn_nout = 0;
			rnp->rn_access = 0;
			return;
		}
	}

	index = -1;
	memcpy(bad_ident, knp->kn_ident, IDENT_LEN);
	for (i = 0; i < MAX_PEER_COUNT; i++) {
		rnp = &rcp->rc_nodes[i];
		if (kad_less_than(rcp->rc_target,
					bad_ident, rnp->kn_ident)) {
			memcpy(bad_ident, rnp->kn_ident, IDENT_LEN);
			index = i;
		}
	}

	if (index != -1) {
		rnp = &rcp->rc_nodes[index];
		rcp->rc_total++;
		knode_copy(rnp, knp);
		rnp->rn_access = 0;
		rnp->rn_type = 1;
		rnp->rn_nout = 0;
		return;
	}

	return;
}

int kad_search_update(int tid, const char *ident, btcodec *codec)
{
	int i;
	int updated;
	size_t elen;
	in_addr in_addr1;
	u_short in_port1;
	btfastvisit btfv;
	const char *self;
	const char *node, *nodes;
	struct kad_node knode;
	struct recursive_node * rnp;
	struct recursive_context * rcp;

	elen = 0;
	updated = 0;
	kad_get_ident(&self);
	nodes = btfv(codec).bget("r").bget("nodes").c_str(&elen);

	for (node = nodes; elen >= 26; node += 26, elen -= 26) {
		memcpy(&in_addr1, node + 20, sizeof(in_addr1));
		memcpy(&in_port1, node + 24, sizeof(in_port1));
		if (in_addr1.s_addr && in_port1 && memcmp(self, node, IDENT_LEN)) {
			knode.kn_type = 1;
			knode.kn_addr.kc_addr = in_addr1;
			knode.kn_addr.kc_port = in_port1;
			memcpy(knode.kn_ident, node, IDENT_LEN);

			for (rcp = _search_slot; rcp; rcp = rcp->rc_next) {
				kad_recursive_update(rcp, &knode);
				if (rcp->rc_tid == tid && updated == 0) {
					for (i = 0; i < MAX_PEER_COUNT; i++) {
						rnp = &rcp->rc_nodes[i];
						if (memcmp(rnp->kn_ident, ident, IDENT_LEN) == 0) {
							//printf("update ident by %d %d\n", rnp->rn_nout, rcp->rc_sentout);
							rnp->rn_type = 2;
							break;
						}
					}
					waitcb_cancel(&rcp->rc_timeout);
					waitcb_switch(&rcp->rc_timeout);
					kad_bound_update(rcp, ident);
					rcp->rc_acked++;
					updated = 1;
				}
			}

			kad_node_insert(&knode);
		}
	}

	if (updated == 0 && (tid & 0x80) == 0x80) {
		printf("ident cannot update: %d\n", tid);
	}

	return 0;
}

struct recursive_context *kad_recursivecb_new(int type, const char *ident)
{
	int i;
	static int tid = 0x0;
	struct recursive_node * rnp;
	struct recursive_context *rcp;

	rcp = new struct recursive_context;

	rcp->rc_tid = (tid++ & 0x7F) | 0x80;
	rcp->rc_type = type;
	rcp->rc_acked = 0;
	rcp->rc_total = 1;
	rcp->rc_sentout = 0;
	memcpy(rcp->rc_target, ident, 20);
	waitcb_init(&rcp->rc_timeout, kad_recursive_output, rcp);

	for (i = 1; i < 8; i++)
		rcp->rc_well_nodes[i].kn_type = 0;

	for (i = 0; i < MAX_PEER_COUNT; i++) {
		rnp = &rcp->rc_nodes[i];
		rnp->rn_access = 0;
		rnp->rn_type = 0;
		rnp->rn_nout = 0;
	}

	rcp->rc_next = _search_slot;
	rcp->rc_prev = &_search_slot;
	if (_search_slot != NULL)
		_search_slot->rc_prev = &rcp->rc_next;
	_search_slot = rcp;
	return rcp;
}

int kad_recursive(int type, const char *ident)
{
	int i;
	int error;
	struct kad_node node2s[8];
	struct recursive_node *rnp;
	struct recursive_context *rcp;

	rcp = kad_recursivecb_new(type, ident);
	assert(rcp != NULL);

	error = kad_krpc_closest(ident, node2s, 8);
	for (i = 0; i < 8; i++) {
		if (!node2s[i].kn_type) 
			continue;

		rnp = &rcp->rc_nodes[i];
		rnp->rn_type = 1;
		knode_copy(rnp, &node2s[i]);
	}

	kad_recursive_output(rcp);
	return 0;
}

