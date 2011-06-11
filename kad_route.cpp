#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <winsock.h>

#include "module.h"
#include "callout.h"
#include "slotwait.h"
#include "kad_proto.h"
#include "kad_route.h"

#define KN_UNKOWN     0x00
#define KN_SEEN       0x01
#define KN_GOOD       0x02

#define MAX_BUCKET_COUNT    (20 * 8)

struct kad_node {
	int kn_flag;
	int kn_seen;
	int kn_fail;
	in_addr kn_addr;
	u_short kn_port;
	char kn_ident[20];
	struct waitcb kn_timeout;
};

struct kad_bucket {
	int kb_flag;
	int kb_seen;
	char kb_mask[20];
	char kb_ident[20];
	struct waitcb kb_timeout;
	struct kad_node kb_swap[8];
	struct kad_node kb_nodes[8];
};

struct kad_bucket _route_bucket[20 * 8];

static int do_next_insert(int type, const char *ident,
	   	in_addr in_addr1, u_short in_port1)
{
	fprintf(stderr, "do next insert\n");
	return 0;
}

static int do_node_scan(struct kad_node *knps[3],
	   	const char *ident, in_addr in_addr1, u_short in_port1)
{
   	int i, j;
	int index;
	struct kad_node *knp;
	struct kad_bucket *kbp, *kbp1;

	knps[0] = knps[1] = knps[2] = 0;

	index = kad_get_bucket(ident);
	for (i = 0; i <= index; i++) {
		kbp = &_route_bucket[i];
		if (kbp->kb_flag & NF_GOOD) {
			kbp1 = kbp;
			continue;
		}

		assert(index <= 120);
	   	break;
	}

	for (j = 0; j < 8; j++) {
	   	knp = &kbp1->kb_nodes[j];

		if (knp->kn_flag == 0) {
		   	knps[2] = knp; // KNP_DEAD;
		   	continue;
	   	}

		if (knp->kn_port == in_port1 &&
			   	knp->kn_addr.s_addr == in_addr1.s_addr) {
		   	knps[0] = knp; // KNP_ROUTE;
	   	}

		if (!memcmp(ident, knp->kn_ident, 20)) {
		   	knps[1] = knp; // KNP_IDENT;
	   	}

		if (knp->kn_flag & NF_DEAD) {
		   	knps[2] = knp; // KNP_DEAD;
	   	}

		if (knps[1] && knps[0]) {
		   	break;
	   	}
	}

	return 0;
}

static int do_node_insert(int type, const char *ident,
	   	in_addr in_addr1, u_short in_port1)
{
	struct kad_node *knp;
	struct kad_node *knps[3];
	struct kad_node *knp_dead;
	struct kad_node *knp_route;
	struct kad_node *knp_ident;

	do_node_scan(knps, ident, in_addr1, in_port1);
	knp_dead  = knps[2];
	knp_ident = knps[1];
	knp_route = knps[0];

	if (knp_ident != NULL) {
		knp = knp_ident;
	} else if (knp_route != NULL) {
		knp = knp_route;
	} else if (knp_dead != NULL) {
		knp = knp_dead;
		knp->kn_flag = KN_UNKOWN;
	} else {
		do_next_insert(type, ident, in_addr1, in_port1);
		return 0;
	}

	if (type == KN_GOOD ||
			(type == KN_SEEN &&
			 (knp->kn_flag & NF_GOOD))) {
	   	waitcb_cancel(&knp->kn_timeout);
	   	knp->kn_seen = GetTickCount();
		knp->kn_flag = KN_GOOD;
	   	knp->kn_fail = 0;
   	}

	if (knp_route != knp) {
		if (knp_route != 0)
			knp_route->kn_flag = 0;
	   	knp->kn_addr = in_addr1;
	   	knp->kn_port = in_port1;
	}

	if (knp_ident != knp) {
		if (knp_ident != 0)
			knp_ident->kn_flag = 0;
		memcpy(knp->kn_ident, ident, 20);
	}

	return 0;
}

static int closest_update(const char *ident,
	   	struct kad_node2 *closest, size_t count, struct kad_node *knp)
{
	int i;
	int index = -1;
	struct kad_node2 bad;
	struct kad_node2 *knp2;

	for (i = 0; i < count; i++) {
		knp2 = &closest[i];
		if (knp2->kn_flag == 0) {
		   	memcpy(knp2->kn_ident, knp->kn_ident, 20);
			knp2->kn_port = knp->kn_port;
			knp2->kn_addr = knp->kn_addr;
			knp2->kn_flag = knp->kn_flag;
			return 0;
		}
	}

	memcpy(bad.kn_ident, knp->kn_ident, 20);
	bad.kn_flag = knp->kn_flag;

	for (i = 0; i < count; i++) {
		knp2 = &closest[i];
		if (bad.kn_flag > knp2->kn_flag) {
			memcpy(bad.kn_ident, knp2->kn_ident, 20);
			bad.kn_flag = knp2->kn_flag;
			index = i;
		} else if (bad.kn_flag == knp2->kn_flag &&
				kad_less_than(ident, bad.kn_ident, knp2->kn_ident)) {
			memcpy(bad.kn_ident, knp2->kn_ident, 20);
			bad.kn_flag = knp2->kn_flag;
			index = i;
		}
	}

	if (index != -1) {
		knp2 = &closest[index];
		memcpy(knp2->kn_ident, knp->kn_ident, 20);
		knp2->kn_port = knp->kn_port;
		knp2->kn_addr = knp->kn_addr;
		knp2->kn_flag = knp->kn_flag;
		return 0;
	}

	return 0;
}

int kad_node_closest(const char *ident, struct kad_node2 *closest, size_t count)
{
	int i, j;
	int min15;
	struct kad_node *knp;
	struct kad_node2 *knp2;
	struct kad_bucket *kbp;

	min15 = 1000 * 15 * 60;
	for (i = 0; i < count; i++)
		closest[i].kn_flag = 0;

	for (i = 0; i < MAX_BUCKET_COUNT; i++) {
		kbp = &_route_bucket[i];
		for (j = 0; j < 8; j++) {
		   	knp = &kbp->kb_nodes[j];
		   	if (knp->kn_flag == 0)
			   	continue;

			if ((knp->kn_flag & NF_GOOD) &&
				   	(knp->kn_seen + min15) < GetTickCount())
			   	knp->kn_flag |= NF_DOUBT;
		   	closest_update(ident, closest, count, knp);
		}
	}

	return 0;
}

int kad_compat_closest(const char *ident, void *buf, size_t len)
{
	int i;
	char *bufp = 0;
	size_t count = 0;
	struct kad_node2 *knp2;
	struct kad_node2 nodes[8];

	kad_node_closest(ident, nodes, 8);

	bufp = (char *)buf;
	for (i = 0; i < 8; i++) {
		knp2 = (nodes + i);
		if (len >= 26 &&
				(knp2->kn_flag != 0)) {
			memcpy(bufp, knp2->kn_ident, 20);
			memcpy(bufp + 20, &knp2->kn_addr, 4);
			memcpy(bufp + 24, &knp2->kn_port, 2);
			count += 26;
			bufp += 26;
			len -= 26;
		}
	}

	return count;
}

int kad_node_good(const char *ident, in_addr in_addr1, u_short in_port1)
{
	do_node_insert(KN_GOOD, ident, in_addr1, in_port1);
	return 0;
}

int kad_node_seen(const char *ident, in_addr in_addr1, u_short in_port1)
{
	do_node_insert(KN_SEEN, ident, in_addr1, in_port1);
	return 0;
}

int kad_node_insert(const char *ident, in_addr in_addr1, u_short in_port1)
{
	do_node_insert(KN_UNKOWN, ident, in_addr1, in_port1);
	return 0;
}

int kad_node_timed(const char *ident, in_addr in_addr1, u_short in_port1)
{
	struct kad_node *knp;
	struct kad_node *knps[3];

	do_node_scan(knps, ident, in_addr1, in_port1);

	knp = NULL;
	if (knps[0] == knps[1]) {
		knp = knps[0];
	}
   
	if (knps[0] != knp)
		callout_reset(&knps[0]->kn_timeout, 2000);

#if 0
	if (knps[1] != knp)
		callout_reset(&knps[1]->kn_timeout, 2000);
#endif

	if (knp != NULL)
		callout_reset(&knp->kn_timeout, 2000);

	return 0;
}

static void kad_node_failure(void *upp)
{
	struct kad_node *knp;

	knp = (struct kad_node *)upp;
	if (++knp->kn_fail > 3) {
		fprintf(stderr, "node is dead %p\n", knp);
		knp->kn_flag |= NF_DEAD;
	}
}

static void kad_bucket_failure(void *upp)
{
	struct kad_bucket *kbp;

	kbp = (struct kad_bucket *)upp;

}

static void module_init(void)
{
	int i, j;
	struct kad_node *knp;
	struct kad_bucket *kbp;

	for (i = 0; i < MAX_BUCKET_COUNT; i++) {
		kbp = &_route_bucket[i];
		waitcb_init(&kbp->kb_timeout, kad_bucket_failure, kbp);
		for (j = 0; j < 8; j++) {
		   	knp = &kbp->kb_swap[j];
		   	waitcb_init(&knp->kn_timeout, kad_node_failure, knp);
		   	knp = &kbp->kb_nodes[j];
		   	waitcb_init(&knp->kn_timeout, kad_node_failure, knp);
		}
	}

	_route_bucket[0].kb_flag = NF_GOOD;
}

static void module_clean(void)
{
	int i, j;
	struct kad_node *knp;
	struct kad_bucket *kbp;

	for (i = 0; i < MAX_BUCKET_COUNT; i++) {
		kbp = &_route_bucket[i];
		waitcb_clean(&kbp->kb_timeout);
		for (j = 0; j < 8; j++) {
		   	knp = &kbp->kb_swap[j];
		   	waitcb_clean(&knp->kn_timeout);
		   	knp = &kbp->kb_nodes[j];
		   	waitcb_clean(&knp->kn_timeout);
		}
	}
}

struct module_stub kad_route_mod = {
	module_init, module_clean
};

