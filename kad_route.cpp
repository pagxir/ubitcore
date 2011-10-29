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

#define K 8
#define KN_DEAD       0x01
#define KN_UNKOWN     0x02
#define KN_QUIET      0x03
#define KN_SEEN       0x04
#define KN_GOOD       0x05

#define MAX_FAILURE 3
#define MAX_BUCKET_COUNT 40

struct kad_item:
	public kad_node
{
	int kn_flag;
	int kn_seen;
	int kn_query;
	int kn_access;
};

struct kad_bucket
{
	int kb_flag;
	int kb_tick;
	struct waitcb kb_ping;
	struct waitcb kb_find;
	struct kad_node kb_cache;
	struct kad_item kb_nodes[K];
	struct kad_node *kb_pinging;
};

static int _r_count = 0;
static int _r_failure = 0;
static struct waitcb _r_bootup;
static const unsigned MIN15 = 60 * 15;
static const unsigned MIN15U = 1000 * 60 * 15;
static struct kad_bucket _r_bucket[MAX_BUCKET_COUNT];

static char _curid[21] = {
	"abcdefghij0123456789"
};

int kad_set_ident(const char *ident)
{
	memcpy(_curid, ident, IDENT_LEN);
	return 0;
}

int kad_get_ident(const char **ident)
{
	*ident = _curid;
	return 0;
}

static int kad_xor_dist(const char *ident)
{
	int i, n, of;
	const char *ip;

	ip = (const char *)ident;
	for (i = 0; i < IDENT_LEN; i++) {

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

static int wire_linkup(void)
{
	struct kad_item *kip;
	struct kad_bucket *kbp;

	if (_r_failure > 48) {
		printf("network change to linkup\n");
		_r_failure = 0;
		return 0;
	}

	_r_failure = 0;
	kbp = _r_bucket + _r_count;
	while (kbp-- > _r_bucket) {
		kbp->kb_pinging = 0;
		kip = kbp->kb_nodes + K;
		while (kip-- > kbp->kb_nodes) {
			kip->kn_access = 0;
			kip->kn_query = 0;
		}
	}

	return 0;
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
	int flag;

	dp->kn_type = src->kn_type;
	dp->kn_addr = src->kn_addr;
	flag = (dp->kn_type == KN_GOOD);
	memcpy(dp->kn_ident, src->kn_ident, IDENT_LEN);
	return NF_ITEM| (flag? NF_HELO: 0);
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

static int do_node_insert(struct kad_node *knp)
{
	int i;
	kad_node node;
	kad_item *kip;
	kad_bucket *kbp;

	unsigned now = 0;
	kad_item *kip1, *kip2, *kip3;

	now = GetTickCount() / 1000;
	kbp = kad_get_bucket(knp);

	kip1 = kip2 = kip3 = NULL;
	kip = kbp->kb_nodes + K;
	while (kip-- > kbp->kb_nodes) {
		if (kip->kn_flag == 0) {
			kip1 = kip;
			continue;
		}

		if (!memcmp(kip->kn_ident, knp->kn_ident, IDENT_LEN)) {
			kip1 = kip;
			break;
		}

		if ((kip2 == NULL || kip->kn_query > kip2->kn_query) &&
				kip->kn_query > MAX_FAILURE && kip->kn_access + 5 < now) {
			kip2 = kip;
			continue;
		}

		if (kip2 == NULL && !(kip->kn_flag & NF_HELO) &&
				(kip->kn_query && kip->kn_access + 5 < now)) {
			kip2 = kip;
			continue;
		}

		if ((knp->kn_type == KN_GOOD) &&
				(kip2 == NULL) && !(kip->kn_flag & NF_HELO)) {
			kip2 = kip;
			continue;
		}

		if (kip3 == NULL || kip3->kn_seen > kip->kn_seen) {
			kip3 = kip;
			continue;
		}
	}

	if (kip + 1 > kbp->kb_nodes) {
		int oldflag = kip->kn_flag;

		switch (knp->kn_type) {
			case KN_SEEN:
				if (oldflag & NF_HELO) {
					kip->kn_seen = now;
					kip->kn_query = 0;
				}
				break;

			case KN_GOOD:
				kip->kn_flag |= NF_HELO;
				kip->kn_seen  = now;
				kip->kn_query = 0;
				kip->kn_access = 0;
				break;
		}

		if (oldflag != kip->kn_flag && NULL != kbp->kb_pinging) {
			callout_reset(&kbp->kb_find, kad_rand(MIN15U, MIN15U/5));
			kbp->kb_pinging = NULL;
			node = kbp->kb_cache;
			node.kn_type = KN_UNKOWN;
			kbp->kb_cache.kn_type = 0;
			do_node_insert(&node);
		}

		return 0;
	}

	if (kip2 != NULL || kip1 != NULL) {
		kip = (kip1? kip1: kip2);
		kip->kn_flag = knode_copy(kip, knp);
		kip->kn_seen = (knp->kn_type == KN_GOOD? now: 0);
		kip->kn_query = 0;
		kip->kn_access = 0;

		if (knp->kn_type != KN_GOOD) {
			send_node_ping(kip);
			kbp->kb_pinging = kip;
			kip->kn_flag |= NF_PING;
			kip->kn_query++;
			return 0;
		}

		callout_reset(&kbp->kb_find, kad_rand(MIN15U, MIN15U/5));
		return 0;
	}

	if (kip3 != NULL && 
			(kip3->kn_seen + MIN15 < now || (kip3->kn_flag & NF_HELO) == 0)) {
		if (kip3->kn_access + 5 < now) {
			send_node_ping(kip3);
			kip3->kn_query++;
		}

		kip3->kn_flag |= NF_PING;
		kbp->kb_pinging = kip3;
		//printf("ping_bucket %d\n", kbp - _r_bucket);

		int kn_type = kbp->kb_cache.kn_type;
		if (knp->kn_type == KN_GOOD)
			kbp->kb_cache = *knp;
		else if (kn_type == 0)
			kbp->kb_cache = *knp;
		else if (9 > rand() % 17 && kn_type != KN_GOOD)
			kbp->kb_cache = *knp;
		return 0;
	}

	if (kbp + 1 == _r_bucket + _r_count) {
		_r_count++;
		for (i = 0; i < K; i++) {
			kip = &kbp->kb_nodes[i];
			if (kip->kn_flag != 0 &&
					kad_get_bucket(kip) != kbp) {
				(kbp + 1)->kb_nodes[i] = *kip;
				kip->kn_flag = 0;
			}
		}

		callout_reset(&_r_bootup, kad_rand(100000, 20000));
		callout_reset(&kbp->kb_find, kad_rand(60000, 30000));
		callout_reset(&kbp[1].kb_find, kad_rand(60000, 30000));
		do_node_insert(knp);
		return 0;
	}

	return 0;
}

static int closest_update(const char *ident, 
		kad_node *closest, size_t count, const kad_node *knp)
{
	int i;
	int num;
	int index = -1;
	struct kad_node bad;
	struct kad_node *knp2;

	for (i = 0; i < count; i++) {
		knp2 = &closest[i];
		if (knp2->kn_type == 0) {
			knode_copy(knp2, knp);
			goto calculate;
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
	}

calculate:
	num = 0;
	for (i = 0; i < count; i++) {
		knp2 = &closest[i];
		if (knp2->kn_type >= KN_QUIET)
			num = num + 1;
	}

	return num;
}

int kad_krpc_closest(const char *ident, struct kad_node *closest, size_t count)
{
	int now;
	int i, j, ind, total;
	struct kad_item *knp;
	struct kad_bucket *kbp;

	now = GetTickCount() / 1000;
	ind = kad_xor_dist(ident);
	for (i = 0; i < count; i++)
		closest[i].kn_type = 0;

	total = 0;
	if (ind < _r_count) {
		kbp = _r_bucket + ind;
		for (j = 0; j < K; j++) {
			knp = &kbp->kb_nodes[j];
			if (knp->kn_flag & NF_ITEM) {
				knp->kn_type = KN_GOOD;
				if (knp->kn_seen + MIN15 < now)
					knp->kn_type = KN_QUIET;
				if ((knp->kn_flag & NF_HELO) == 0)
					knp->kn_type = KN_UNKOWN;
				if (knp->kn_query > MAX_FAILURE)
					knp->kn_type = KN_DEAD;
				total = closest_update(ident, closest, count, knp);
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
		for (j = 0; j < K; j++) {
			knp = &kbp->kb_nodes[j];
			if (knp->kn_flag & NF_ITEM) {
				knp->kn_type = KN_GOOD;
				if (knp->kn_seen + MIN15 < now)
					knp->kn_type = KN_QUIET;
				if ((knp->kn_flag & NF_HELO) == 0)
					knp->kn_type = KN_UNKOWN;
				if (knp->kn_query > MAX_FAILURE)
					knp->kn_type = KN_DEAD;
				total = closest_update(ident, closest, count, knp);
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
	struct kad_node nodes[K], *knp;
	kad_krpc_closest(ident, nodes, K);

	bufp = (char *)buf;
	for (i = 0; i < K; i++) {
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
	wire_linkup();
	return 0;
}

int kad_node_seen(struct kad_node *knp)
{
	knp->kn_type = KN_SEEN;
	do_node_insert(knp);
	wire_linkup();
	return 0;
}

int kad_node_insert(struct kad_node *knp)
{
	knp->kn_type = KN_UNKOWN;
	do_node_insert(knp);
	return 0;
}

int kad_node_timed(struct kad_node *knp, const char *title)
{
	int i;
	int now;
	struct kad_item *kip;
	struct kad_bucket *kbp;

	kbp = kad_get_bucket(knp);
	for (i = 0; i < K; i++) {
		kip = &kbp->kb_nodes[i];
		if (kip->kn_flag == 0)
			continue;
		if (!memcmp(kip->kn_ident, knp->kn_ident, IDENT_LEN))
			break;
	}

	if (i < K) { 
		callout_reset(&kbp->kb_ping, kad_rand(10000, 5000));
		now = GetTickCount() / 1000;
		kip->kn_access = now;
		kip->kn_flag |= NF_PING;
		kip->kn_query++;
	}

	char buf[41];
	printf("[O] %s %s %s:%d\n",
		title, hex_encode(buf, knp->kn_ident, IDENT_LEN),
		inet_ntoa(knp->kn_addr.kc_addr), htons(knp->kn_addr.kc_port));
	return 0;
}

static void kad_ping_failure(void *upp)
{
	kad_node node;
	kad_bucket *kbp;

	kbp = (struct kad_bucket *)upp;

	if (++_r_failure == 48) {
		// TODO: do failure rollback;
		printf("network down!\n");
	}

	if (kbp->kb_cache.kn_type) {
		node = kbp->kb_cache;
		node.kn_type = KN_UNKOWN;
		kbp->kb_cache.kn_type = 0;
		kbp->kb_pinging = NULL;
		do_node_insert(&node);
	}

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
	kad_findnode(node);

	callout_reset(&_r_bootup, kad_rand(MIN15U, MIN15U/3));
}

static void kad_find_failure(void *upp)
{
	int now;
	int ind;
	int mask;
	char node[20];
	const char *ident;
	struct kad_item *kip;
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

	now = GetTickCount() / 1000;
	for (int i = 0; i < K; i++) {
		kip = &kbp->kb_nodes[i];
		if (kip->kn_query > MAX_FAILURE ||
				kip->kn_seen + MIN15 < now ||
				(kip->kn_flag & NF_HELO) == 0) {
			callout_reset(&kbp->kb_find, kad_rand(60000, 30000));
			printf("kad_bucket_failure: %d\n", ind);
			send_bucket_update(node);
			return;
		}
	}

	callout_reset(&kbp->kb_find, kad_rand(MIN15U, MIN15U/5));
	return;
}

static const char *kad_flag(struct kad_item *kip)
{
	int now;
	char *p;
	static char buf[8] = "";

	p = buf;
	now = GetTickCount() / 1000;
	*p++ = (kip->kn_flag & NF_PING)? 'P': '_';
	*p++ = (kip->kn_flag & NF_HELO)? 'H': '_';
	*p++ = (kip->kn_query && kip->kn_access + 5 < now)? 'F': '_';
	*p++ = 0;

	return buf;
}

int store_value_dump(void);
int kad_route_dump(int index)
{
	int i, j, now;
	char identstr[41];
	struct kad_item *knp;
	struct kad_bucket *kbp;

	now = GetTickCount() / 1000;
	for (i = 0; i < _r_count; i++) {
		int good = 0;
		int port = 0;
		in_addr addr;
		kbp = _r_bucket + i;

		for (j = 0; j < K; j++) {
			knp = kbp->kb_nodes + j;
			if (knp->kn_flag == 0) {
				continue;
			}

			if ((knp->kn_query == 0) &&
					(knp->kn_flag & NF_HELO) &&
					(knp->kn_seen + MIN15 > now))
				good++;

			addr = knp->kn_addr.kc_addr;
			port = knp->kn_addr.kc_port;
			printf("%s %s  %4d %s:%d\n", kad_flag(knp),
					hex_encode(identstr, knp->kn_ident, IDENT_LEN),
					(now - knp->kn_seen), inet_ntoa(addr), htons(port));
		}

		kbp->kb_tick = kbp->kb_find.wt_value / 1000;
		printf("route-%02d %d%s\n", i, kbp->kb_tick - now, good == K? "(FULL)": "");
	}

	printf("route total %d, bootup %d, failure %d\n",
			_r_count, (_r_bootup.wt_value/1000 - now), _r_failure);
	store_value_dump();
	return 0;
}

static void module_init(void)
{
	int i, j;
	struct kad_item *knp;
	struct kad_bucket *kbp;

	for (i = 0; i < MAX_BUCKET_COUNT; i++) {
		kbp = _r_bucket + i;
		waitcb_init(&kbp->kb_ping, kad_ping_failure, kbp);
		waitcb_init(&kbp->kb_find, kad_find_failure, kbp);
		memset(kbp->kb_nodes, 0, sizeof(kbp->kb_nodes));
		kbp->kb_flag = 0;
		kbp->kb_tick = 0;
	}

	_r_count = 1;
	kbp = &_r_bucket[0];
	callout_reset(&kbp->kb_find, kad_rand(MIN15U/5, MIN15U/6));

	waitcb_init(&_r_bootup, kad_bootup_update, NULL);
	callout_reset(&_r_bootup, kad_rand(MIN15U/5, MIN15U/6));
}

static void module_clean(void)
{
	int i, j;
	struct kad_item *knp;
	struct kad_bucket *kbp;

	waitcb_clean(&_r_bootup);
	for (i = 0; i < MAX_BUCKET_COUNT; i++) {
		kbp = _r_bucket + i;
		waitcb_clean(&kbp->kb_find);
		waitcb_clean(&kbp->kb_ping);
	}
}

struct module_stub kad_route_mod = {
	module_init, module_clean
};

