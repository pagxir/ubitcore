#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <winsock.h>

#include "timer.h"
#include "btcodec.h"
#include "slotwait.h"
#include "slotsock.h"
#include "recursive.h"
#include "proto_kad.h"
#include "udp_daemon.h"

static int kad_less_than(const char *sp, const char *lp, const char *rp)
{
	int i;
	uint8_t l, r;

	i = 0;
	while (*lp == *rp) {
		if (i++ < 20) {
		   	lp++, rp++;
			continue;
		}
		break;
	}

	if (i < 20) {
	   	l = (*sp ^ *lp) & 0xFF;
	   	r = (*sp ^ *rp) & 0xFF;
	   	return (l < r);
	}

	return 0;
}

static int kad_bound_check(struct recursive_context *rcp, const char *node)
{
	int i;
	struct recursive_well *rwp;

	for (i = 0; i < 8; i++) {
		if (rcp->rc_well_nodes[i].rn_flags == 0)
			return 1;

		rwp = &rcp->rc_well_nodes[i];
		if (kad_less_than(rcp->rc_target, node, rwp->rn_ident))
			return 1;
	}

	return 0;
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
		if (rnp->rn_count < perf[i]->rn_count ||
				(rnp->rn_count == perf[i]->rn_count &&
				 kad_less_than(ident, rnp->rn_ident, perf[i]->rn_ident))) {
			perf[i] = rnp;
			return;
		}
	}
}

static void kad_recursive_output(void *upp)
{
	int i, j;
	int error = -1;
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
		if (rnp->rn_flags == 1) {
			if (rnp->rn_touch + 1000 > curtick) {
				error = 0;
			} else if (!kad_bound_check(rcp, rnp->rn_ident)) {
				continue;
			} else if (rnp->rn_count < 3 || rcp->rc_acked < 8) {
				kad_node_perf(rnp->rn_ident, rnp, perf, MAX_SEND_OUT);
			}
		}
	}

	for (i = 0; i < MAX_SEND_OUT; i++) {
		rnp = perf[i];
		if (perf[i] != NULL) {
			waitcb_cancel(&rnp->rn_wait);
			so_addr.sin_family = AF_INET;
			so_addr.sin_port   = rnp->rn_port;
			so_addr.sin_addr   = rnp->rn_addr;
			error = kad_proto_out(rcp->rc_type, rcp->rc_target, &so_addr,
					rnp->rn_response, sizeof(rnp->rn_response), &rnp->rn_wait);

			if (error == -1)
				break;

			if (error == 1)
				return;

		   	rcp->rc_touch = curtick;
			rnp->rn_touch = curtick;
			rcp->rc_sentout++;
			rnp->rn_count++;
			error = 0;
		}
	}

	callout_reset(&rcp->rc_timeout, 1000);

	if (error == -1) {
		fprintf(stderr, "recursive dump: %d/%d/%d\n",
				rcp->rc_total, rcp->rc_sentout, rcp->rc_acked);
		for (i = 0; i < MAX_PEER_COUNT; i++)
			waitcb_clean(&rcp->rc_nodes[i].rn_wait);
		waitcb_clean(&rcp->rc_timeout);
		delete rcp;
	}

	return;
}

static void kad_bound_update(struct recursive_context *rcp,
		const char *node, in_addr in_addr1, u_short in_port1)
{
	int i;
	int index = -1;
	char bad_ident[20];
	struct recursive_well *rwp;

	for (i = 0; i < 8; i++) {
		rwp = &rcp->rc_well_nodes[i];
		if (rwp->rn_flags == 1) {
			if (!memcmp(rwp->rn_ident, node, 20))
				return;

			if (rwp->rn_port == in_port1 &&
				   	rwp->rn_addr.s_addr == in_addr1.s_addr)
				return;
		}
	}

	for (i = 0; i < 8; i++) {
		rwp = &rcp->rc_well_nodes[i];
		if (rwp->rn_flags == 0) {
			memcpy(rwp->rn_ident, node, 20);
			rwp->rn_port = in_port1;
			rwp->rn_addr = in_addr1;
			rwp->rn_flags = 1;
			return;
		}
	}

	memcpy(bad_ident, node, 20);
	for (i = 0; i < 8; i++) {
		rwp = &rcp->rc_well_nodes[i];
		if (kad_less_than(rcp->rc_target,
				   	bad_ident, rwp->rn_ident)) {
			memcpy(bad_ident, rwp->rn_ident, 20);
			index = i;
		}
	}

	if (index != -1) {
		rwp = &rcp->rc_well_nodes[index];
		memcpy(rwp->rn_ident, node, 20);
		rwp->rn_port = in_port1;
		rwp->rn_addr = in_addr1;
		rwp->rn_flags = 1;
		return;
	}
}

static void kad_recursive_update(struct recursive_context *rcp,
		const char *node, in_addr in_addr1, u_short in_port1)
{
	int i;
	int less_count = 0;
	struct recursive_node *rnp;

	if (!kad_bound_check(rcp, node)) {
		/* this node is bound out. */
	   	return;
	}

	for (i = 0; i < MAX_PEER_COUNT; i++) {
		rnp = &rcp->rc_nodes[i];
		if (rnp->rn_flags != 0) {
			if (rnp->rn_port == in_port1 &&
					rnp->rn_addr.s_addr == in_addr1.s_addr)
				return;
			if (!memcmp(rnp->rn_ident, node, 20))
				return;
		}
	}

	for (i = 0; i < MAX_PEER_COUNT; i++) {
		rnp = &rcp->rc_nodes[i];
		if (rnp->rn_flags == 0 ||
				(rnp->rn_count == 1 && rnp->rn_flags == 2) ||
				(rnp->rn_flags == 2 && rnp->rn_touch + 2000 < GetTickCount())) {
			rcp->rc_total++;
			rnp->rn_flags = 1;
			rnp->rn_count = 0;
			rnp->rn_touch = 0;
			rnp->rn_addr = in_addr1;
			rnp->rn_port = in_port1;
			memcpy(rnp->rn_ident, node, 20);
			return;
		}
	}

	fprintf(stderr, "warn: too small buffer!\n");
	return;
}

static void kad_recursive_input(void *upp)
{
	int i;
	int needouptut;

	size_t elen;
	btcodec codec;
	in_addr in_addr1;
	u_short in_port1;
	const char *node, *nodes;
	struct recursive_node * rnp;
	struct recursive_context * rcp;

	rcp = (struct recursive_context *)upp;

	needouptut = 0;
	for (i = 0; i < MAX_PEER_COUNT; i++) {
		rnp = (struct recursive_node *)&rcp->rc_nodes[i];
		if (waitcb_completed(&rnp->rn_wait)) {
			rcp->rc_acked++;
			rnp->rn_flags = 2;
			codec.parse(rnp->rn_response, rnp->rn_wait.wt_count);

			if (rnp->rn_count == 1 &&
					rnp->rn_touch == rcp->rc_touch)
				needouptut = 1;

			nodes = codec.bget().bget("r").bget("id").c_str(&elen);
			if (nodes == NULL) {
				waitcb_clear(&rnp->rn_wait);
				continue;
			}

			kad_bound_update(rcp, nodes, in_addr1, in_port1);
			nodes = codec.bget().bget("r").bget("nodes").c_str(&elen);
			if (nodes == NULL) {
				waitcb_clear(&rnp->rn_wait);
				continue;
			}

			for (node = nodes; elen >= 26; node += 26, elen -= 26) {
				memcpy(&in_addr1, node + 20, sizeof(in_addr1));
				memcpy(&in_port1, node + 24, sizeof(in_port1));
				kad_recursive_update(rcp, node, in_addr1, in_port1);
			}

			waitcb_clear(&rnp->rn_wait);
		}
	}

	if (needouptut)
		kad_recursive_output(rcp);

	return;
}

int kad_recursive(int type, const char *ident, const char *server)
{
	int i;
	int error;
	struct sockaddr_in so_addr;
	struct recursive_node *rnp;
	struct recursive_context *rcp;

	error = getaddrbyname(server, &so_addr);
	assert(error == 0);

	rcp = new struct recursive_context;

	rcp->rc_type = type;
	rcp->rc_acked = 0;
	rcp->rc_total = 1;
	rcp->rc_sentout = 0;
	memcpy(rcp->rc_target, ident, 20);
	waitcb_init(&rcp->rc_timeout, kad_recursive_output, rcp);

	for (i = 1; i < 8; i++)
		rcp->rc_well_nodes[i].rn_flags = 0;

	for (i = 0; i < MAX_PEER_COUNT; i++) {
		rnp = &rcp->rc_nodes[i];
		rnp->rn_flags = 0;
		rnp->rn_count = 0;
		rnp->rn_touch = 0;
		waitcb_init(&rnp->rn_wait, kad_recursive_input, rcp);
	}

	rnp = &rcp->rc_nodes[0];
	rnp->rn_flags = 1;
	rnp->rn_addr = so_addr.sin_addr;
	rnp->rn_port = so_addr.sin_port;

	kad_recursive_output(rcp);
	return 0;
}

