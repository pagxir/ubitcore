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

#define KN_UNKOWN     0x01
#define KN_SEEN       0x02
#define KN_GOOD       0x03

#define MAX_BUCKET_COUNT    (20 * 8)

struct kad_item:
	public kad_node
{
	int kn_flag;
	int kn_seen, kn_fail;
	struct waitcb kn_timeout;
};

struct kad_bucket {
	struct waitcb kb_timeout;
	struct kad_item *kb_pinged;
	struct kad_item kb_nodes[16];
};

static int _r_count = 0;
static int _r_failure = 0;
static struct waitcb _r_bootup;
static const unsigned MIN15 = 1000 * 60 * 15;
static struct kad_bucket _r_bucket[MAX_BUCKET_COUNT];

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

static int kad_rand(int base, int adjust)
{
	int update = rand();
	if (base > (adjust / 2))
		base -= (adjust / 2);
	if (adjust > 0)
		update %= adjust;
	return base + update;
}

static kad_item *kad_node_perf(struct kad_bucket *kbp)
{
	unsigned now;
	kad_item *kip;
	kad_item *newkip = NULL;

	now = 0;
	for (int i = 0; i < 16; i++) {
		kip = &kbp->kb_nodes[i];
		if ((kip->kn_flag & NF_ITEM) == 0 &&
				(kip->kn_flag & NF_NODE) && (kip->kn_fail == 0)) {
			if (newkip == NULL) {
				newkip = kip;
				continue;
			}

			if ((kip->kn_flag & NF_HELO) >
					(newkip->kn_flag & NF_HELO)) {
				newkip = kip;
				continue;
			}

			if (kip->kn_seen > kip->kn_seen) {
				newkip = kip;
				continue;
			}
		}
	}

	return newkip;
}

static int do_node_ping(struct kad_item *kip)
{ 
	unsigned now = GetTickCount();

	if (kip->kn_seen + MIN15 < now ||
			(kip->kn_flag & NF_HELO) == 0) {
		if (!waitcb_active(&kip->kn_timeout)) {
			callout_reset(&kip->kn_timeout, 2000);
			send_node_ping(kip);
		}
		return 1;
	}

	return 0;
}

static kad_item *kad_find_replace(kad_bucket *kbp, kad_node *knp)
{
	int oldflag = 0;
	kad_item *kip, *oldkip = NULL;

	for (int i = 0; i < 16; i++) {
		kip = &kbp->kb_nodes[i];
		if (knp != NULL &&
				!memcmp(kip->kn_ident, knp->kn_ident, IDENT_LEN)) {
			oldflag = kip->kn_flag;
			return kip;
		}

		if (oldkip == NULL) {
			oldflag = kip->kn_flag;
			oldkip = kip;
			continue;
		}

		if ((oldflag & NF_ITEM) !=
				(kip->kn_flag & NF_ITEM)) {
			if ((oldflag & NF_ITEM) >
					(kip->kn_flag & NF_ITEM)) {
				oldflag = kip->kn_flag;
				oldkip = kip;
			}
			continue;
		}

		if ((oldflag & NF_HELO) !=
				(kip->kn_flag & NF_HELO)) {
			if ((oldflag & NF_HELO) >
					(kip->kn_flag & NF_HELO)) {
				oldflag = kip->kn_flag;
				oldkip = kip;
			}
			continue;
		}

		if ((oldflag & NF_NODE) !=
				(kip->kn_flag & NF_NODE)) {
			if ((oldflag & NF_NODE) >
					(kip->kn_flag & NF_NODE)) {
				oldflag = kip->kn_flag;
				oldkip = kip;
			}
			continue;
		}

		if (oldkip->kn_fail < kip->kn_fail) {
			oldflag = kip->kn_flag;
			oldkip = kip;
			continue;
		}

		if (oldkip->kn_seen > kip->kn_seen) {
			oldflag = kip->kn_flag;
			oldkip = kip;
			continue;
		}
	}

	return oldkip;
}

static int kad_bucket_adjust(struct kad_bucket *kbp)
{
	int i;
	int now;
	int good = 0;
	int nitem = 0;
	kad_item *kip;
	kad_item *kip1;
	kad_item *kip2;
	kad_bucket *lastkbp;

	now = GetTickCount();
	for (i = 0; i < 16; i++) {
		kip = &kbp->kb_nodes[i];
		if (kip->kn_flag == 0) {
			continue;
		}

		if (kip->kn_flag & NF_ITEM) {
			if (kip->kn_fail >= 3) { 
				kip1 = kad_node_perf(kbp);
				if (kip1 != NULL) {
					callout_reset(&kbp->kb_timeout, kad_rand(MIN15, MIN15/3));
					kip1->kn_flag |= NF_ITEM;
					kip->kn_flag = 0;
				}
			}
			nitem++;
		}

		if ((kip->kn_flag & NF_HELO) &&
				(kip->kn_seen + MIN15 > now)) {
			good++;
		}
	}

	lastkbp = &_r_bucket[_r_count];
	if (good > 8 && kbp + 1 == lastkbp) {
		kad_item *kip1 = _r_bucket[_r_count++].kb_nodes;
		for (int i = 0; i < 16; i++) {
			kip = &kbp->kb_nodes[i];
			if ((kip->kn_flag & NF_NODE) &&
					lastkbp == kad_get_bucket(kip)) {
				kip1->kn_fail = kip->kn_fail;
				kip1->kn_seen = kip->kn_seen;
				kip1->kn_flag = kip->kn_flag;
				waitcb_cancel(&kip->kn_timeout);
				knode_copy(kip1, kip);
				kip->kn_flag = 0;
				kip1++;
			}
		}

		callout_reset(&lastkbp->kb_timeout, kad_rand(60 * 1000, 30 * 1000));
		callout_reset(&kbp->kb_timeout, kad_rand(60 * 1000, 30 * 1000));
		callout_reset(&_r_bootup, kad_rand(MIN15, MIN15/3));
		kad_bucket_adjust(lastkbp);
		kad_bucket_adjust(kbp);
		return 0;
	}

	while (nitem < 8) {
		kip = kad_node_perf(kbp);
		if (kip == NULL)
			break;
		printf("replace node: %d\n", kbp - _r_bucket);
		callout_reset(&kbp->kb_timeout, kad_rand(MIN15, MIN15/3));
		kip->kn_flag |= NF_ITEM;
		nitem++;
	}

	/* find a node need to ping. */

	good = 0;
	kip1 = kip2 = 0;
	for (i = 0; i < 16; i++) {
		kip = &kbp->kb_nodes[i];
		const int want = NF_NODE| NF_HELO;
		const int mask = NF_NODE| NF_HELO| NF_ITEM;

		if ((kip->kn_fail == 0) &&
				(kip->kn_seen + MIN15 > now) &&
				((kip->kn_flag & mask) == want)) {
			good++;
		}

		while (kip->kn_flag & NF_ITEM) {
			if (kip1 == NULL) {
				kip1 = kip;
				break;
			}

			if ((kip1->kn_flag & NF_HELO) !=
					(kip->kn_flag & NF_HELO)) {
				if ((kip1->kn_flag & NF_HELO) >
						(kip->kn_flag & NF_HELO))
					kip1 = kip;
				break;
			}

			if (kip1->kn_seen > kip->kn_seen) {
				kip1 = kip;
				break;
			}

			break;
		}

		const int want1 = NF_NODE;
		const int mask1 = NF_NODE| NF_ITEM;
		while ((kip->kn_flag & mask1) == want1) {
			if (kip2 == NULL) {
				kip2 = kip;
				break;
			}
			if ((kip2->kn_flag & NF_HELO) !=
					(kip->kn_flag & NF_HELO)) {
				if ((kip2->kn_flag & NF_HELO) >
						(kip->kn_flag & NF_HELO))
					kip1 = kip;
				break;
			}

			if (kip2->kn_seen > kip->kn_seen) {
				kip2 = kip;
				break;
			}

			break;
		}
	}

	if (good == 0) {
		if (kip2 != NULL) {
			do_node_ping(kip2);
			kbp->kb_pinged = kip2;
		}
		return 0;
	}

	if (kip1 == NULL ||
			((kip1->kn_flag & NF_HELO) &&
			 (kip1->kn_seen + MIN15 > now))) {
		for (int i = 0; i < 16; i++) {
			kip = &kbp->kb_nodes[i];
			if (kip->kn_flag & NF_ITEM)
				continue;
			kip->kn_flag = 0;
		}
		return 0;
	}

	do_node_ping(kip1);
	kbp->kb_pinged = kip1;
	return 0;
}

static int do_node_insert(struct kad_node *knp)
{
	kad_item *kip;
	kad_bucket *kbp;
	unsigned now = 0;

	now = GetTickCount();
	kbp = kad_get_bucket(knp);

	kip = kad_find_replace(kbp, knp);
	if ((kip != NULL) &&
			(kip->kn_flag & NF_NODE) &&
			!memcmp(kip->kn_ident, knp->kn_ident, IDENT_LEN)) {
		int oldflag = kip->kn_flag;

		switch (knp->kn_type) {
			case KN_SEEN:
				if (oldflag & NF_HELO) {
					kip->kn_seen = now;
					kip->kn_fail = 0;
				}
				break;

			case KN_GOOD:
				waitcb_cancel(&kip->kn_timeout);
				kip->kn_flag |= NF_HELO;
				kip->kn_seen = now;
				kip->kn_fail = 0;
				break;
		}

		if ((now == kip->kn_seen) &&
				(kip->kn_flag & NF_ITEM) && kip == kbp->kb_pinged) {
			callout_reset(&kbp->kb_timeout, kad_rand(MIN15, MIN15/3));
			kbp->kb_pinged = 0;
		}
		kad_bucket_adjust(kbp);
		return 0;
	}

	if (kip != NULL && !(kip->kn_flag & NF_ITEM)) {
		if ((kip->kn_flag & NF_NODE) &&
				(kip->kn_fail < 3) && knp->kn_type != KN_GOOD) {
			unsigned now = GetTickCount();
			if ((NF_HELO & ~kip->kn_flag) || (kip->kn_seen + MIN15 < now)) {
				 if (!waitcb_active(&kip->kn_timeout)) {
					 send_node_ping(kip);
					return 0;
				}
				send_node_ping(knp);
			}
			return 0;
		}

		waitcb_cancel(&kip->kn_timeout);
		kip->kn_flag = knode_copy(kip, knp);
		kip->kn_seen = now;
		kip->kn_fail = 0;
		kad_bucket_adjust(kbp);
		return 0;
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
	int i, j, ind, total;
	struct kad_item *knp;
	struct kad_bucket *kbp;

	ind = kad_xor_dist(ident);
	for (i = 0; i < count; i++)
		closest[i].kn_type = 0;

	total = 0;
	if (ind < _r_count) {
		kbp = _r_bucket + ind;
		for (j = 0; j < 16; j++) {
			knp = &kbp->kb_nodes[j];
			if ((knp->kn_flag & NF_ITEM) && knp->kn_fail < 3) {
				knp->kn_type = KN_GOOD;
				if ((knp->kn_flag & NF_HELO) &&
						(knp->kn_seen + MIN15) < GetTickCount())
					knp->kn_type = KN_UNKOWN;
				closest_update(ident, closest, count, knp);
				total++;
			}
		}

		if (total >= count) {
			return 0;
		}
	}

	for (i = _r_count; i > 0; i--) {
		if (ind >= i - 1 && total >= count) {
			return 0;
		}

		if (ind == i - 1) {
			continue;
		}

		kbp = &_r_bucket[i - 1];
		for (j = 0; j < 16; j++) {
			knp = &kbp->kb_nodes[j];
			if ((knp->kn_flag & NF_ITEM) && knp->kn_fail < 3){
				knp->kn_type = KN_GOOD;
				if ((knp->kn_flag & NF_HELO) &&
						(knp->kn_seen + MIN15) < GetTickCount())
					knp->kn_type = KN_UNKOWN;
				closest_update(ident, closest, count, knp);
				total++;
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
			memcpy(bufp + 24, &knp->kn_addr.kc_port, 2);
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
	if (_r_failure >= 48) 
		printf("network recover\n");
	_r_failure = 0;
	return 0;
}

int kad_node_seen(struct kad_node *knp)
{
	knp->kn_type = KN_SEEN;
	do_node_insert(knp);
	if (_r_failure >= 48) 
		printf("network recover\n");
	_r_failure = 0;
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

	callout_reset(&kip->kn_timeout, 5000);
	return 0;
}

static void kad_node_failure(void *upp)
{
	kad_item *knp;
	kad_bucket *kbp;

	knp = (struct kad_item *)upp;
	knp->kn_fail++;

	if (knp->kn_flag & NF_HELO) {
		if (++_r_failure == 48) {
			// TODO: do failure rollback;
			printf("network down!\n");
		}
	}

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

static void kad_bootup_update(void *upp)
{
	char node[20];
	const char *ident;

	kad_get_ident(&ident);
	memcpy(node, ident, IDENT_LEN);
	node[IDENT_LEN - 1] ^= 0x1;
	send_bucket_update(node);

	callout_reset(&_r_bootup, kad_rand(MIN15, MIN15/3));
}

static void kad_bucket_failure(void *upp)
{
	int ind;
	int mask;
	char node[20];
	const char *ident;
	struct kad_bucket *kbp;

	kbp = (struct kad_bucket *)upp;

	ind = kbp - _r_bucket;
	rnd_set_array(node);
	kad_get_ident(&ident);

	memcpy(node, ident, ind / 8);
	mask = 1 << (7 - (ind % 8));
	node[ind / 8] &= (mask - 1);
	node[ind / 8] |= (ident[ind / 8] & ~(mask - 1));
	node[ind / 8] ^= mask;

	send_bucket_update(node);
	printf("kad_bucket_failure: %d\n", ind);
	callout_reset(&kbp->kb_timeout, kad_rand(60 * 1000, 30 * 1000));
}

static const char *convert_flags(int flags)
{
	char *p;
	static char buf[8] = "";

	p = buf;
	*p++ = (flags & NF_ITEM)? 'I': '_';
	*p++ = (flags & NF_HELO)? 'H': '_';
	*p++ = 0;
	
	return buf;
}

int kad_route_dump(int index)
{
	int i, j;
	KAC *kacp;
	char identstr[41];
	struct kad_item *knp;
	struct kad_bucket *kbp;

	int now = GetTickCount();
	if (index < _r_count && index >= 0) {
		kbp = _r_bucket + index;

		for (j = 0; j < 16; j++) {
			knp = kbp->kb_nodes + j;
			if (knp->kn_flag & NF_NODE) {
				kacp = &knp->kn_addr;
				printf("%s %s  %4d(%d) %s:%d\n",
						convert_flags(knp->kn_flag),
						hex_encode(identstr, knp->kn_ident, IDENT_LEN),
						(now - knp->kn_seen) / 1000, knp->kn_fail,
						inet_ntoa(kacp->kc_addr), htons(kacp->kc_port));
			}
		}

		return 0;
	}

	for (i = 0; i < _r_count; i++) {
		kbp = _r_bucket + i;
		printf("route-%02d %d\n", i, (kbp->kb_timeout.wt_value - now) / 1000);

		for (j = 0; j < 16; j++) {
			knp = kbp->kb_nodes + j;
			if (knp->kn_flag & NF_ITEM) {
				kacp = &knp->kn_addr;
				printf("%s  %4d %s:%d\n",
						hex_encode(identstr, knp->kn_ident, IDENT_LEN),
						(now - knp->kn_seen) / 1000,
						inet_ntoa(kacp->kc_addr), htons(kacp->kc_port));
			}
		}
	}

	printf("route total %d, bootup %d, failure %d\n",
		_r_count, (_r_bootup.wt_value - now) / 1000, _r_failure);
	return 0;
}

static void module_init(void)
{
	int i, j;
	struct kad_item *knp;
	struct kad_bucket *kbp;

	for (i = 0; i < MAX_BUCKET_COUNT; i++) {
		kbp = _r_bucket + i;

		waitcb_init(&kbp->kb_timeout, kad_bucket_failure, kbp);
		kbp->kb_pinged = 0;
		for (j = 0; j < 16; j++) {
			knp = kbp->kb_nodes + j;
			waitcb_init(&knp->kn_timeout, kad_node_failure, knp);
			knp->kn_flag = 0;
		}
	}

	_r_count = 1;
	kbp = &_r_bucket[0];
	callout_reset(&kbp->kb_timeout, MIN15);

	waitcb_init(&_r_bootup, kad_bootup_update, NULL);
	callout_reset(&_r_bootup, kad_rand(MIN15/5, MIN15/6));
}

static void module_clean(void)
{
	int i, j;
	struct kad_item *knp;
	struct kad_bucket *kbp;

	waitcb_clean(&_r_bootup);
	for (i = 0; i < MAX_BUCKET_COUNT; i++) {
		kbp = _r_bucket + i;
		waitcb_clean(&kbp->kb_timeout);

		for (j = 0; j < 16; j++) {
			knp = kbp->kb_nodes + j;
			waitcb_clean(&knp->kn_timeout);
		}
	}
}

struct module_stub kad_route_mod = {
	module_init, module_clean
};

