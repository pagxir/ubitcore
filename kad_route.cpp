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
	struct waitcb kb_timeout;
	struct kad_node kb_kitty;
	struct kad_node kb_nodes[16];
};

static int _route_count = 0;
struct kad_bucket _route_bucket[20 * 8];
const unsigned MIN15 = 1000 * 15 * 60;

static struct kad_node *
replace_swap_node(int type, in_addr in_addr1,
		u_short in_port1, const char *ident)
{
	int i, ind;
	kad_node *knp;
	kad_node *knp1;
	kad_node *knp2;
	kad_bucket *kbp1;
	ind = kad_get_bucket(ident);
	kbp1 = &_route_bucket[ind < _route_count? ind: _route_count - 1];

	/* If full of unkown node, just ping it. avoid dup node in route. */
	knp1 = knp2 = NULL;
	for (i = 8; i < 16; i++) {
		knp = &kbp1->kb_nodes[i];
		if (knp->kn_flag == 0) {
			knp1 = knp;
			continue;
		}

		knp2 = (knp->kn_flag & NF_GOOD)? knp2: knp;
		if (0 == memcmp(knp->kn_ident, ident, 20)) {
			knp->kn_flag = (type == KN_GOOD? NF_GOOD: knp->kn_flag);
			knp->kn_seen = GetTickCount();
			return knp->kn_flag & NF_GOOD? NULL: knp;
		}
	}

	if (knp1 != NULL) {
		knp = knp1;
	   	knp->kn_addr = in_addr1;
	   	knp->kn_port = in_port1;
	   	knp->kn_fail = 0;
	   	knp->kn_flag = (type == KN_GOOD? NF_GOOD: NF_UNKOWN);
	   	knp->kn_seen = GetTickCount();
	   	waitcb_cancel(&knp->kn_timeout);
	   	memcpy(knp->kn_ident, ident, 20);
		return knp->kn_flag & NF_GOOD? NULL: knp;
	}

	if (knp2 != NULL && type == KN_GOOD) {
		knp = knp2;
	   	knp->kn_addr = in_addr1;
	   	knp->kn_port = in_port1;
	   	knp->kn_fail = 0;
	   	knp->kn_flag = NF_GOOD;
	   	knp->kn_seen = GetTickCount();
	   	waitcb_cancel(&knp->kn_timeout);
	   	memcpy(knp->kn_ident, ident, 20);
		return NULL;
	}

	if (knp2 != NULL) {
		knp = &kbp1->kb_kitty;
	   	knp->kn_addr = in_addr1;
	   	knp->kn_port = in_port1;
	   	knp->kn_fail = 0;
	   	knp->kn_flag = NF_GOOD;
	   	knp->kn_seen = GetTickCount();
	   	waitcb_cancel(&knp->kn_timeout);
	   	memcpy(knp->kn_ident, ident, 20);
		return knp;
	}

	return NULL;
}

static int replace_live_node(struct kad_node *knp1)
{
	int ind;
	kad_node *knp;
	kad_bucket *kbp1;
	ind = kad_get_bucket(knp->kn_ident);
	kbp1 = &_route_bucket[ind < _route_count? ind: _route_count - 1];

	for (ind = 8; ind < 16; ind++) {
		knp = &kbp1->kb_nodes[ind];
		if (knp->kn_flag & NF_GOOD) {
			knp->kn_flag = 0;
			waitcb_cancel(&knp->kn_timeout);
			waitcb_cancel(&knp1->kn_timeout);
			*knp = *knp1;
			return 1;
		}
	}

	for (ind = 8; ind < 16; ind++) {
		knp = &kbp1->kb_nodes[ind];
		if (knp->kn_flag & NF_UNKOWN) {
			knp->kn_flag = 0;
			waitcb_cancel(&knp->kn_timeout);
			waitcb_cancel(&knp1->kn_timeout);
			*knp = *knp1;
			return 1;
		}
	}

	return 0;
}

static int have_swap_node(struct kad_node *knp)
{
	int ind;
	kad_bucket *kbp1;
	ind = kad_get_bucket(knp->kn_ident);
	kbp1 = &_route_bucket[ind < _route_count? ind: _route_count - 1];

	for (ind = 8; ind < 16; ind++) {
		knp = &kbp1->kb_nodes[ind];
		if (knp->kn_flag & NF_GOOD)
			return 1;
	}

	return 0;
}

void do_send_ping(in_addr addr1, u_short port1);
static int do_node_ping(struct kad_node *knp)
{
	if ((knp->kn_flag & NF_GOOD) == 0 ||
		   	knp->kn_seen + MIN15 < GetTickCount()) {
		printf("output node ping %p!\n", knp);
	   	callout_reset(&knp->kn_timeout, 2000);
	   	do_send_ping(knp->kn_addr, knp->kn_port);
	}

	return 0;
}

static int do_node_scan(struct kad_node *knps[3],
		const char *ident, in_addr in_addr1, u_short in_port1)
{
	int j;
	int index;
	struct kad_node *knp;
	struct kad_bucket *kbp1;

	knps[0] = knps[1] = knps[2] = 0;

	index = kad_get_bucket(ident);
	kbp1 = &_route_bucket[index < _route_count? index: _route_count - 1];

	for (j = 0; j < 8; j++) {
		knp = &kbp1->kb_nodes[j];

		if (knp->kn_flag == 0) {
			knps[2] = knp; // KNP_DEAD;
			continue;
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

static int do_split_bucket(struct kad_bucket *kbp)
{
	int i;
	int index;
	struct kad_node *knp;
	struct kad_bucket *kbp1;

	index = _route_count;
	if (kbp != &_route_bucket[index - 1])
		return -1;

	index = _route_count++;
	assert(index < 120);
	kbp1 = &_route_bucket[index];

	for (i = 0; i < 8; i++) {
		knp = &kbp->kb_nodes[i];
		if (index == kad_get_bucket(knp->kn_ident)) {
			waitcb_cancel(&knp->kn_timeout);
			waitcb_cancel(&kbp1->kb_nodes[i].kn_timeout);
			kbp1->kb_nodes[i] = *knp;
			knp->kn_flag = 0;
		}
	}

	callout_reset(&kbp1->kb_timeout, MIN15);
	printf("do split bucket!\n");
	return 0;
}

static int try_split_bucket(struct kad_node *knp)
{
	int ind;
	kad_bucket *kbp1;
	ind = kad_get_bucket(knp->kn_ident);
	kbp1 = &_route_bucket[ind < _route_count? ind: _route_count - 1];
	if (++ind == _route_count)
		do_split_bucket(kbp1);
	return 0;
}

static int do_next_insert(int type, const char *ident,
		in_addr in_addr1, u_short in_port1)
{
	int j;
	int index;
	struct kad_node *knp;
	struct kad_node *knp1;
	struct kad_node *knp2;
	struct kad_node *knp3;
	struct kad_bucket *kbp1;

	index = kad_get_bucket(ident);
	kbp1 = &_route_bucket[index < _route_count? index: _route_count - 1];

	knp1 = knp2 = knp3 = 0;
	for (j = 0; j < 8; j++) {
		knp = &kbp1->kb_nodes[j];
		if ((knp->kn_flag & NF_DEAD) == NF_DEAD) {
			knp3 = knp;
			continue;
		}

		if ((knp->kn_flag & NF_GOOD) != NF_GOOD) {
			knp1 = knp;
			continue;
		}

		if (knp->kn_seen + MIN15 < GetTickCount()) {
			knp2 = knp;
			continue;
		}
	}

	knp = NULL;
	if (knp3 != NULL)
		knp = knp3;
	else if (type == KN_GOOD)
		knp = knp1;

	if (knp == NULL) {
		if (type == KN_GOOD && knp2 == NULL) {
			if (do_split_bucket(kbp1) == 0)
				return 1;
			return 0;
		}

		knp = replace_swap_node(type, in_addr1, in_port1, ident);
		if (knp2 != NULL)
			do_node_ping(knp2);
		else if (knp != NULL)
			do_node_ping(knp);
		return 0;
	}

	waitcb_cancel(&knp->kn_timeout);
	knp->kn_seen = GetTickCount();
	knp->kn_flag = NF_UNKOWN;
	knp->kn_fail = 0;
	knp->kn_addr = in_addr1;
	knp->kn_port = in_port1;
	memcpy(knp->kn_ident, ident, 20);
	if (type == KN_GOOD) 
		knp->kn_flag |= NF_GOOD;

	callout_reset(&kbp1->kb_timeout, MIN15);
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
		knp->kn_flag = NF_UNKOWN;
	} else {
		int retval = do_next_insert(type, ident, in_addr1, in_port1);
		if (retval == 1)
			do_node_insert(type, ident, in_addr1, in_port1);
		return 0;
	}

	if (type == KN_GOOD ||
			(type == KN_SEEN &&
			 (knp->kn_flag & NF_GOOD))) {
		waitcb_cancel(&knp->kn_timeout);
		knp->kn_seen = GetTickCount();
		knp->kn_flag = KN_GOOD;
		knp->kn_fail = 0;
		try_split_bucket(knp);
		//callout_reset(&kbp1->kn_timeout, MIN15);
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
	struct kad_node *knp;
	struct kad_node2 *knp2;
	struct kad_bucket *kbp;

	for (i = 0; i < count; i++)
		closest[i].kn_flag = 0;

	for (i = 0; i < MAX_BUCKET_COUNT; i++) {
		kbp = &_route_bucket[i];
		for (j = 0; j < 8; j++) {
			knp = &kbp->kb_nodes[j];
			if (knp->kn_flag == 0)
				continue;

			if ((knp->kn_flag & NF_GOOD) &&
					(knp->kn_seen + MIN15) < GetTickCount())
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
		fprintf(stderr, "node is dead %p, do replace node\n", knp);
		knp->kn_flag |= NF_DEAD;
		replace_live_node(knp);
		return;
	}

	if (have_swap_node(knp))
		do_node_ping(knp);

	return;
}

static void kad_bucket_failure(void *upp)
{
	struct kad_bucket *kbp;

	kbp = (struct kad_bucket *)upp;
	printf("kad_bucket_failure\n");
}

static void module_init(void)
{
	int i, j;
	struct kad_node *knp;
	struct kad_bucket *kbp;

	for (i = 0; i < MAX_BUCKET_COUNT; i++) {
		kbp = &_route_bucket[i];
		waitcb_init(&kbp->kb_timeout, kad_bucket_failure, kbp);
		knp = &kbp->kb_kitty;
		waitcb_init(&knp->kn_timeout, kad_node_failure, knp);
		for (j = 0; j < 16; j++) {
			knp = &kbp->kb_nodes[j];
			waitcb_init(&knp->kn_timeout, kad_node_failure, knp);
		}
		knp = &kbp->kb_kitty;
		waitcb_init(&knp->kn_timeout, kad_node_failure, knp);
	}

	_route_count = 1;
	kbp = &_route_bucket[0];
	callout_reset(&kbp->kb_timeout, MIN15);
}

static void module_clean(void)
{
	int i, j;
	struct kad_node *knp;
	struct kad_bucket *kbp;

	for (i = 0; i < MAX_BUCKET_COUNT; i++) {
		kbp = &_route_bucket[i];
		waitcb_clean(&kbp->kb_timeout);
		for (j = 0; j < 16; j++) {
			knp = &kbp->kb_nodes[j];
			waitcb_clean(&knp->kn_timeout);
		}
	}
}

struct module_stub kad_route_mod = {
	module_init, module_clean
};

