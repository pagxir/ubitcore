#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <winsock.h>

#include "utils.h"
#include "module.h"
#include "callout.h"
#include "slotwait.h"
#include "kad_proto.h"
#include "kad_route.h"
#include "udp_daemon.h"

#define KN_SEEN       0x01
#define KN_GOOD       0x02
#define KN_UNKOWN     0x03

#define MAX_BUCKET_COUNT    (20 * 8)

struct kad_item:
	public kad_node
{
	int kn_flag;
	int kn_seen;
	int kn_fail;
	struct waitcb kn_timeout;
};

struct kad_bucket {
	struct waitcb kb_timeout;
	struct kad_item kb_nodes[16];
};

static int _r_count = 0;
const unsigned MIN15 = 1000 * 60 * 5;
struct kad_bucket _r_bucket[MAX_BUCKET_COUNT];

static char _curid[21] = {
	"abcdefghij0123456789"
};

int kad_set_ident(const char *ident)
{
	memcpy(_curid, ident, 20);
	return 0;
}

int kad_get_ident(const char **ident)
{
	*ident = _curid;
	return 0;
}

int kad_xor_dist(const char *ident)
{
	int i, n, of;
	const char *ip;

	ip = (const char *)ident;
	for (i = 0; i < 20; i++) {

		if (ip[i] == _curid[i])
			continue;

		n = 0;
		of = (ip[i] ^ _curid[i]) & 0xFF;
		if (of >> 4) {
			of >>= 4;
			n += 4;
		}

		if (of >> 2) {
			of >>= 2;
			n += 2;
		}

		if (of >> 1) {
			of >>= 1;
			n += 1;
		}

		return (i + 1) * 8 - of - n;
	}

	return 120;
}

static kad_bucket *kad_get_bucket(const kad_node *kp)
{
	int ind;

	ind = kad_xor_dist(kp->kn_ident);
	if (ind < _r_count)
		return _r_bucket + ind;

	assert(_r_count > 0);
	return _r_bucket + _r_count - 1;
}

static int knode_copy(struct kad_node *dp, const struct kad_node *src)
{
	dp->kn_type = src->kn_type;
	dp->kn_addr = src->kn_addr;
	memcpy(dp->kn_ident, src->kn_ident, IDENT_LEN);
	if (dp->kn_type == KN_GOOD)
		return (NF_HELO| NF_NODE);
	return NF_NODE;
}

static kad_item *kad_node_find(struct kad_node *knp)
{
	int i;
	struct kad_item *kip;
	struct kad_bucket *kbp;

	kbp = kad_get_bucket(knp);
	for (i = 0; i < 16; i++) {
		kip = &kbp->kb_nodes[i];
		if (kip->kn_flag != 0 &&
				!memcmp(kip->kn_ident, knp->kn_ident, IDENT_LEN))
			return kip;
	}

	return 0;
}

static int do_node_ping(struct kad_item *kip)
{
	if (kip != NULL) {
	   	callout_reset(&kip->kn_timeout, 2000);
	   	send_node_ping(kip);
	}
	return 0;
}

static int kad_bucket_adjust(struct kad_bucket *kbp)
{
	int i;
	int now;
	int good = 0;
	int nitem = 0;
	kad_item *kip, *doubtp, *unkownp;

	now = GetTickCount();
	doubtp = unkownp = 0;
	for (i = 0; i < 16; i++) {
		kip = &kbp->kb_nodes[i];
		if (kip->kn_flag == 0 || kip->kn_fail >= 3) {
			kip->kn_flag = 0;
			continue;
		}

		if (kip->kn_flag & NF_ITEM) {
			nitem++;
		}

		if ((kip->kn_flag & NF_HELO) &&
			(kip->kn_flag & NF_ITEM) == 0 &&
				(kip->kn_seen + MIN15 > now)) {
			/* node is good. */
			good++;
		} else if (!waitcb_active(&kip->kn_timeout)) {
			if (kip->kn_flag & NF_ITEM) {
				if (doubtp == NULL ||
						kip->kn_seen < doubtp->kn_seen)
				   	doubtp = kip;
			} else if (unkownp == NULL ||
					kip->kn_seen < unkownp->kn_seen) {
				unkownp = kip;
			}
		}
	}

	if (nitem >= 8) {
		if (good < 8 && unkownp) {
			printf("do_node_ping doubtp\n");
			do_node_ping(unkownp);
			return 0;
		} else if (good > 0 && doubtp) {
			printf("do_node_ping doubtp\n");
			do_node_ping(doubtp);
			return 0;
		}

		for (i = 0; i < 16; i++) {
			kip = &kbp->kb_nodes[i];
			if ((kip->kn_flag & NF_ITEM) == 0)
				continue;

			if ((kip->kn_flag & NF_HELO) == 0 || 
					(kip->kn_seen + MIN15) < now) {
				return 0;
			}
		}

		kip = &kbp->kb_nodes[0];
		for (i = 0; i < 16; i++, kip++) {
			if (kip->kn_flag & NF_ITEM)
				continue;
			kip->kn_flag = 0;
		}

		printf("all node is good now: %d\n", kbp - _r_bucket);
		return 0;
	}

	for (i = 0; i < 16 && nitem < 8; i++) {
		kip = &kbp->kb_nodes[i];
		if (kip->kn_flag == 0 || kip->kn_fail >= 3) {
			kip->kn_flag = 0;
			continue;
		}

		if ((kip->kn_flag & NF_HELO) &&
			(kip->kn_flag & NF_ITEM) == 0) {
		   	callout_reset(&kbp->kb_timeout, MIN15);
			kip->kn_flag |= NF_ITEM;
			nitem++;
			continue;
		}

		if (good < 8 &&
			(kip->kn_flag & NF_ITEM) == 0) {
		   	callout_reset(&kbp->kb_timeout, MIN15);
			kip->kn_flag |= NF_ITEM;
			nitem++, good++;
			continue;
		}
	}

	return 0;
}

static int do_node_insert(struct kad_node *knp)
{
	int i;
	int good = 0;
	int doublt = 0;
	int unkown = 0;
	unsigned now = 0;
	kad_bucket *kbp;
	kad_item *kip, *deadp, *unstablep;

	now = GetTickCount();
	deadp = unstablep = 0;
	kbp = kad_get_bucket(knp);
	for (i = 0; i < 16; i++) {
		kip = &kbp->kb_nodes[i];
		if (kip->kn_flag == 0 || kip->kn_fail >= 3) {
			kip->kn_flag = 0;
			deadp = kip;
			continue;
		}

		if (!memcmp(kip->kn_ident, knp->kn_ident, IDENT_LEN)) {
			switch (knp->kn_type) {
				case KN_SEEN:
					if (kip->kn_flag & NF_HELO) {
						if (kip->kn_flag & NF_ITEM)
						   	callout_reset(&kbp->kb_timeout, MIN15);
						kip->kn_seen = now;
						kip->kn_fail = 0;
					}
					break;

				case KN_GOOD:
					kip->kn_seen = now;
					kip->kn_fail = 0;
					if (kip->kn_flag & NF_ITEM)
					   	callout_reset(&kbp->kb_timeout, MIN15);
					if (kip->kn_flag & NF_HELO)
						break;
					kip->kn_flag |= NF_HELO;
					kad_bucket_adjust(kbp);
					break;
			}

			waitcb_cancel(&kip->kn_timeout);
			return 0;
		}

		if (kip->kn_seen + MIN15 > now &&
				(kip->kn_flag & NF_HELO) == NF_HELO) {
			good++;
			continue;
		}

		if ((kip->kn_flag & NF_ITEM) &&
				(kip->kn_flag & NF_HELO)) {
			doublt++;
			continue;
		}

		if (unstablep == NULL ||
			   	unstablep->kn_seen < kip->kn_seen)
		   	unstablep = kip;
		unkown++;
		continue;
	}

	kip = deadp? deadp: unstablep;
	if (good < 8 && kip != NULL) {
		if (knp->kn_type == KN_GOOD) { 
			kip->kn_flag = knode_copy(kip, knp);
			kip->kn_fail = 0;
			kip->kn_seen = now;
			waitcb_cancel(&kip->kn_timeout);
			kad_bucket_adjust(kbp);
			return 0;
		}

		if (deadp == NULL) {
			kip->kn_flag = knode_copy(kip, knp);
			kip->kn_fail = 0;
			kip->kn_seen = now;
			waitcb_cancel(&kip->kn_timeout);
			kad_bucket_adjust(kbp);
			return 0;
		}

		struct kad_item kb_kitty;
		kip = &kb_kitty;
		kip->kn_flag = knode_copy(kip, knp);
		kip->kn_fail = 0;
		kip->kn_seen = now;
		waitcb_init(&kip->kn_timeout, 0, 0);
		do_node_ping(kip);
		waitcb_clean(&kip->kn_timeout);
	}

	if (good >= 8 && kbp == _r_bucket + _r_count - 1) {
		int ind = 0;
		struct kad_item *kip1;
		struct kad_bucket *kbp1;

		ind = _r_count++;
		kbp1 = _r_bucket + ind;
		kip1 = &kbp1->kb_nodes[0];
		for (i = 0; i < 16; i++) {
			kip = &kbp->kb_nodes[i];
			if (kad_xor_dist(kip->kn_ident) >= ind &&
				kip->kn_fail < 3 &&	kip->kn_flag != 0) {
				knode_copy(kip1, kip);
				kip1->kn_fail = kip->kn_fail;
				kip1->kn_seen = kip->kn_seen;
				kip1->kn_flag = kip->kn_flag;
				waitcb_cancel(&kip->kn_timeout);
				kip->kn_flag = 0;
				kip1++;
			}
		}

		int update = rand() % MIN15;
		kad_bucket_adjust(kbp1);
		callout_reset(&kbp1->kb_timeout, MIN15 / 2 - update / 3);

		kad_bucket_adjust(kbp);
		do_node_insert(knp);
	}

	return 0;
}

static int closest_update(const char *ident, 
		kad_node *closest, size_t count, const kad_node *knp)
{
	int i;
	int index = -1;
	struct kad_node bad;
	struct kad_node *knp2;

	for (i = 0; i < count; i++) {
		knp2 = &closest[i];
		if (knp2->kn_type == 0) {
			memcpy(knp2->kn_ident, knp->kn_ident, 20);
			knode_copy(knp2, knp);
			return 0;
		}
	}

	knode_copy(&bad, knp);
	for (i = 0; i < count; i++) {
		knp2 = &closest[i];
		if (bad.kn_type > knp2->kn_type) {
			knode_copy(&bad, knp2);
			index = i;
		} else if (bad.kn_type < knp2->kn_type) {
			/* this just ignore. */
			continue;
		} else if (kad_less_than(ident, bad.kn_ident, knp2->kn_ident)) {
			knode_copy(&bad, knp2);
			index = i;
		}
	}

	if (index != -1) {
		knp2 = &closest[index];
		knode_copy(&bad, knp);
		return 0;
	}

	return 0;
}

int kad_krpc_closest(const char *ident, struct kad_node *closest, size_t count)
{
	int i, j;
	struct kad_item *knp;
	struct kad_bucket *kbp;

	for (i = 0; i < count; i++)
		closest[i].kn_type = 0;

	for (i = 0; i < _r_count; i++) {
		kbp = &_r_bucket[i];
		for (j = 0; j < 16; j++) {
			knp = &kbp->kb_nodes[j];
			if ((knp->kn_flag & NF_ITEM) && knp->kn_fail < 3){
				knp->kn_type = KN_GOOD;
				if ((knp->kn_flag & NF_HELO) &&
						(knp->kn_seen + MIN15) < GetTickCount())
					knp->kn_type = KN_UNKOWN;
				closest_update(ident, closest, count, knp);
			}
		}
	}

	return 0;
}

int kad_krpc_closest(const char *ident, char *buf, size_t len)
{
	int i;
	char *bufp = 0;
	size_t count = len;
	struct kad_node nodes[8], *knp;
	kad_krpc_closest(ident, nodes, 8);

	bufp = (char *)buf;
	for (i = 0; i < 8; i++) {
		knp = nodes + i;
		if (len >= 26 && knp->kn_type) {
			memcpy(bufp, knp->kn_ident, IDENT_LEN);
			memcpy(bufp + 20, &knp->kn_addr.kc_addr, 4);
			memcpy(bufp + 20, &knp->kn_addr.kc_port, 2);
			bufp += 26;
			len -= 26;
		}
	}

	return count - len;
}

int kad_node_good(struct kad_node *knp)
{
	knp->kn_type = KN_GOOD;
	do_node_insert(knp);
	return 0;
}

int kad_node_seen(struct kad_node *knp)
{
	knp->kn_type = KN_SEEN;
	do_node_insert(knp);
	return 0;
}

int kad_node_insert(struct kad_node *knp)
{
	knp->kn_type = KN_UNKOWN;
	do_node_insert(knp);
	return 0;
}

int kad_node_timed(struct kad_node *knp)
{
	struct kad_item *kip;

	kip = kad_node_find(knp);
	if (kip == NULL)
		return -1;

	callout_reset(&kip->kn_timeout, 2000);
	return 0;
}

static void kad_node_failure(void *upp)
{
	kad_item *knp;
	kad_bucket *kbp;

	knp = (struct kad_item *)upp;
	knp->kn_fail++;

	kbp = kad_get_bucket(knp);
	kad_bucket_adjust(kbp);
	return;
}

inline size_t bic_init(void)
{
	int bic = 0;
	int biv = RAND_MAX;

	while (biv > 0) {
		biv >>= 8;
		bic++;
	}

	return bic;
}

static int rnd_set_array(char *node)
{
	int salt0;
	size_t mc, rc;
	char *pmem = (char *)node;
	static const int bic = bic_init();

	rc = 0;
	mc = 20;
	while (mc > 0) {
		if (rc == 0) {
			salt0 = rand();
			rc = bic;
		}

		*pmem++ = salt0;
		salt0 >>= 8;
		rc--; 
		mc--;
	}

	return 0;
}

static void kad_bucket_failure(void *upp)
{
	int ind;
	char node[20];
	const char *ident;
	struct kad_bucket *kbp;

	kbp = (struct kad_bucket *)upp;

	ind = kbp - _r_bucket;
	printf("kad_bucket_failure: %d\n", ind);
	rnd_set_array(node);
	kad_get_ident(&ident);
	int update = rand() % MIN15;
	callout_reset(&kbp->kb_timeout, (15 * 1000));
	for (int i = 0; i < ind / 8; i++)
		node[i] = ident[i];

	int mask = 1 << (7 - (ind % 8));
	node[ind / 8] &= (mask - 1);
	node[ind / 8] |= (ident[ind / 8] & ~(mask - 1));
	node[ind / 8] ^= mask;

	char buf[41];
	printf("%d %d %s\n", ind, kad_xor_dist(node),
		   	hex_encode(buf, node, IDENT_LEN));
	assert(ind == kad_xor_dist(node));
	send_bucket_update(node);
}

int kad_route_dump(void)
{
	int i, j;
	KAC *kacp;
	char identstr[41];
	struct kad_item *knp;
	struct kad_bucket *kbp;

	int now = GetTickCount();
	for (i = 0; i < _r_count; i++) {
		kbp = _r_bucket + i;

		printf("route: %d/%d %d@%d\n", i,
			   	_r_count, (kbp->kb_timeout.wt_value - now) / 1000, 
				waitcb_active(&kbp->kb_timeout));
		for (j = 0; j < 16; j++) {
			knp = kbp->kb_nodes + j;
			if (knp->kn_flag & NF_ITEM) {
				kacp = &knp->kn_addr;
				printf("%s %4d %s:%d\n",
					   	hex_encode(identstr, knp->kn_ident, IDENT_LEN),
						(GetTickCount() - knp->kn_seen) / 1000,
					   	inet_ntoa(kacp->kc_addr), htons(kacp->kc_port));
			}
		}
	}

	return 0;
}

static void module_init(void)
{
	int i, j;
	struct kad_item *knp;
	struct kad_bucket *kbp;

	for (i = 0; i < MAX_BUCKET_COUNT; i++) {
		kbp = _r_bucket + i;

		for (j = 0; j < 16; j++) {
			knp = kbp->kb_nodes + j;
			waitcb_init(&knp->kn_timeout, kad_node_failure, knp);
		}

		waitcb_init(&kbp->kb_timeout, kad_bucket_failure, kbp);
	}

	_r_count = 1;
	kbp = &_r_bucket[0];
	callout_reset(&kbp->kb_timeout, MIN15);
}

static void module_clean(void)
{
	int i, j;
	struct kad_item *knp;
	struct kad_bucket *kbp;

	for (i = 0; i < MAX_BUCKET_COUNT; i++) {
		kbp = _r_bucket + i;

		for (j = 0; j < 16; j++) {
			knp = kbp->kb_nodes + j;
			waitcb_clean(&knp->kn_timeout);
		}

		waitcb_clean(&kbp->kb_timeout);
	}
}

struct module_stub kad_route_mod = {
	module_init, module_clean
};

