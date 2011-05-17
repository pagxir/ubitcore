#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <winsock.h>

#include "timer.h"
#include "module.h"
#include "btcodec.h"
#include "slotwait.h"
#include "slotsock.h"
#include "proto_kad.h"
#include "udp_daemon.h"

static FILE * _dmp_file;
static FILE * _log_file;
static int _udp_sockfd = 0;
static struct waitcb _sockcb;
static struct waitcb _stopcb;
static struct waitcb _startcb;

static int _idgen = 0x19821130;
static uint8_t _kadid[20];
static struct waitcb _timer0;

static int _kad_len = 0;
static char _kad_buf[2048];
static struct sockaddr_in _kad_addr;
const int MAX_PEER_COUNT = 2000;

struct one_peer_upp {
   	int gpc_flags;
   	int gpc_count;
   	int gpc_touch;
   	char gpc_ident[20];
   	in_addr gpc_addr0;
   	u_short gpc_port0;
   	char gpc_response[1600];
   	struct waitcb gpc_wait0;
};

struct get_peer_context {
	struct {
		int gpc_flags;
		char gpc_ident[20];
		in_addr gpc_addr0;
		u_short gpc_port0;
	} gpc_goods[8];

	int gpc_acked;
	int gpc_total;
	int gpc_sentout;
	char gpc_target[20];
	struct waitcb gpc_timeout;
	struct one_peer_upp gpc_nodes[MAX_PEER_COUNT];
};

static void hexdump(const char * buf, size_t len)
{
	int i, j, c;

	for (i = 0; i < len; i += 16) {
		c = (len - i < 16? len - i: 16);
	   	fprintf(stderr, "%04x: ", i);
		for (j = 0; j < c; j++)
			fprintf(stderr, "%02X ", buf[i + j] & 0xFF); 
		fprintf(stderr, "%02X\n", buf[i + j] & 0xFF);
	}
}

static slotcb _kad_slot = 0;
static void kad_proto_input(char * buf, size_t len)
{
   	void * idp;
	size_t elen;
	btcodec codec;
	u_short in_port1;
   	in_addr in_addr1;
   	struct waitcb * waitcbp;
	const char *node, *nodes;

	codec.parse(buf, len);

	nodes = codec.bget().bget("t").c_str(&elen);
	if (nodes != NULL && elen == sizeof(void *)) {
		memcpy(&idp, nodes, elen);
		for (waitcbp = _kad_slot; waitcbp;
			   	waitcbp = waitcbp->wt_next) {
			if (waitcbp == idp) {
				if (len >= waitcbp->wt_count)
					fprintf(stderr, "count %d %d\n", len, waitcbp->wt_count);
				assert(len < waitcbp->wt_count);
				waitcbp->wt_count = len;
				memcpy(waitcbp->wt_data, buf, len);
				waitcb_cancel(waitcbp);
				waitcb_switch(waitcbp);
				break;
			}
		}
	}

#if 0
	nodes = codec.bget().bget("r").bget("id").c_str(&elen);
	if (nodes != NULL) {
		fprintf(stderr, "id: ");
		while (elen-- > 0)
			fprintf(stderr, "%02X", 0xFF&*nodes++);
		fprintf(stderr, "\n");
	}

	nodes = codec.bget().bget("r").bget("token").c_str(&elen);
	if (nodes != NULL) {
		fprintf(stderr, "token: ");
		while (elen-- > 0)
			fprintf(stderr, "%02X", 0xFF&*nodes++);
		fprintf(stderr, "\n");
	}
#endif

	int i = 0;
   	nodes = codec.bget().bget("r").bget("values").bget(i++).c_str(&elen);
	while (nodes != NULL) {
	   	for (node = nodes; elen >= 6; node += 6, elen -= 6) {
		   	memcpy(&in_addr1, node + 0, sizeof(in_addr1));
		   	memcpy(&in_port1, node + 4, sizeof(in_port1));
		   	fprintf(stderr, "value %s:%d\n",
				   	inet_ntoa(in_addr1), ntohs(in_port1));
		   	fprintf(_log_file, "value %s:%d\n",
				   	inet_ntoa(in_addr1), ntohs(in_port1));
	   	}
	   	nodes = codec.bget().bget("r").bget("values").bget(i++).c_str(&elen);
	}

#if 0
	nodes = codec.bget().bget("r").bget("nodes").c_str(&elen);
	if (nodes != NULL) {
		int idlen;
		const char * idstr;
		fprintf(stderr, "nodes: \n");
	   	for (node = nodes; elen >= 26; node += 26, elen -= 26) {
		   	memcpy(&in_addr1, node + 20, sizeof(in_addr1));
		   	memcpy(&in_port1, node + 24, sizeof(in_port1));
			idlen = 20;
			for (idstr = node; idlen > 0; idlen--, idstr++)
			   	fprintf(stderr, "%02X", 0xFF & *idstr++);
		   	fprintf(stderr, " %s:%d\n", inet_ntoa(in_addr1), ntohs(in_port1));
	   	}
	}

	fprintf(stderr, "receive packet: %d\n", len);
#endif
	_kad_len = 0;
	return;
}

static void udp_routine(void * upp)
{
	int count, sockfd;
	char sockbuf[2048];
	struct sockaddr_in in_addr1;

	sockfd = _udp_sockfd;
	waitcb_cancel(&_sockcb);
	for ( ; ; ) {
	   	count = recvfrom2(sockfd, sockbuf,
			   	sizeof(sockbuf), &in_addr1, &_sockcb);
		if (count >= 0) {
			kad_proto_input(sockbuf, count);
			fwrite(sockbuf, count, 1, _dmp_file);
		} else if (waitcb_active(&_sockcb)) {
			/* recv is blocking! */
			break;
		} else {
			fprintf(stderr, "recv error %d\n", GetLastError());
			continue;
		}
	}

	return;
}

static void udp_timer(void * upp)
{
	int len;

	if (_kad_len > 0) {
	   	len = sendto2(_udp_sockfd, _kad_buf,
			   	_kad_len, &_kad_addr, &_timer0);
	   	callout_reset(&_timer0, 2000);
	}
}

int kad_auto_send(char * buf, size_t len, struct sockaddr_in * so_addr)
{
	_kad_addr = *so_addr;
	assert(len < sizeof(_kad_buf));
	memcpy(_kad_buf, buf, len);
	_kad_len = len;

	waitcb_cancel(&_timer0);
	udp_timer(0);
	return 0;
}

/*
 * router.utorrent.com
 * router.bittorrent.com:6881
 */
int kad_setident(const char * ident)
{
	uint8_t * uident;

	uident = (uint8_t *)ident;
	kad_set_ident(uident);

	return 0;
}

static int kad_get_peers_send(const char * ident,
		const struct sockaddr_in * so_addr, void * buf,
	   	size_t len0, struct waitcb * waitcbp)
{
	int len;
	int error;
	char sockbuf[2048];
	len = kad_get_peers(sockbuf, sizeof(sockbuf),
		   	(uint32_t)waitcbp, (const uint8_t *)ident);

	error = sendto(_udp_sockfd, sockbuf, len, 0,
		   	(const struct sockaddr *)so_addr, sizeof(*so_addr));
	if (error > 0) {
		waitcbp->wt_data = buf;
		waitcbp->wt_count = len0;
		slot_insert(&_kad_slot, waitcbp);
	}
	return (error == -1? -1: 0);
}

static int ident_less(const char * std0, const char * left, const char * right)
{
	int i;
	uint8_t l, r;

	for (i = 0; i < 20; i++) {
		if (left[i] != right[i]) {
		   	l = (std0[i] ^ left[i]) & 0xFF;
		   	r = (std0[i] ^ right[i]) & 0xFF;
			return (l < r);
		}
	}

	return 0;
}

static int get_peers_bound_check(struct get_peer_context *gpcp, const char * node)
{
	int i;

	for (i = 0; i < 8; i++) {
		if (gpcp->gpc_goods[i].gpc_flags == 0)
			return 1;
		if (ident_less(gpcp->gpc_target,
					node, gpcp->gpc_goods[i].gpc_ident))
			return 1;
	}

	return 0;
}

static void kad_getpeers_output(void * upp)
{
	int i;
	int error = -1;
	int this_send = 0;
	int bound_check = 0;
	int times_check = 0;
	int timeout_check = 0;
	struct sockaddr_in so_addr;
	struct one_peer_upp * oupp;
	struct get_peer_context * gpcp;

	gpcp = (struct get_peer_context *)upp;

	for (i = 0; i < MAX_PEER_COUNT && this_send < 3; i++) {
		oupp = (struct one_peer_upp *)&gpcp->gpc_nodes[i];
		if (oupp->gpc_flags == 1) {

			if (!get_peers_bound_check(gpcp, oupp->gpc_ident)) {
				if (oupp->gpc_touch + 1000 > GetTickCount()) {
					bound_check = 1;
				   	error = 0;
				}
				continue;
			}

			/* if acked < 8, we should let gpc_count large than 5 */
			if (oupp->gpc_count <= 3 &&
				   	oupp->gpc_touch + 1000 <= GetTickCount()) {
			   	waitcb_cancel(&oupp->gpc_wait0);
			   	so_addr.sin_family = AF_INET;
			   	so_addr.sin_port   = oupp->gpc_port0;
			   	so_addr.sin_addr   = oupp->gpc_addr0;
			   	error = kad_get_peers_send(gpcp->gpc_target, &so_addr,
					   	oupp->gpc_response, sizeof(oupp->gpc_response),
					   	&oupp->gpc_wait0);
			   	if (error == -1)
				   	break;
			   	if (error == 1)
				   	return;
				oupp->gpc_touch = GetTickCount();
			   	gpcp->gpc_sentout++;
				oupp->gpc_count++;
			   	this_send++;
			} 

			if (oupp->gpc_count <= 3 ||
				   	oupp->gpc_touch + 1000 > GetTickCount()) {
				times_check = (oupp->gpc_count < 3);
				timeout_check = (oupp->gpc_touch + 1000 > GetTickCount());
			   	error = 0;
		   	}
		}
	}

	callout_reset(&gpcp->gpc_timeout, 1000);
	if (error == -1) {
		fprintf(stderr, "get_peers dump: %d/%d/%d\n",
			   	gpcp->gpc_total, gpcp->gpc_sentout, gpcp->gpc_acked);
		for (i = 0; i < MAX_PEER_COUNT; i++) {
			if (gpcp->gpc_nodes[i].gpc_flags != 0) {
#if 0
				fprintf(stderr, "%s:%d\n",
						inet_ntoa(gpcp->gpc_nodes[i].gpc_addr0),
					   	ntohs(gpcp->gpc_nodes[i].gpc_port0));
#endif
			   	waitcb_cancel(&gpcp->gpc_nodes[i].gpc_wait0);
			}
		}
	   	waitcb_cancel(&gpcp->gpc_timeout);
		fprintf(stderr, "get_peers finish: %d/%d\n",
			   	gpcp->gpc_total, gpcp->gpc_sentout);
		delete gpcp;
	}

	return;
}

static void get_peers_bound(struct get_peer_context *gpcp,
	   	const char *node, in_addr in_addr1, u_short in_port1)
{
	int i;
	int index = -1;
	char bad_ident[20];

	for (i = 0; i < 8; i++) {
		if (gpcp->gpc_goods[i].gpc_flags == 1) {
			if (!memcmp(gpcp->gpc_goods[i].gpc_ident, node, 20))
				return;
			if (gpcp->gpc_goods[i].gpc_port0 == in_port1 &&
					gpcp->gpc_goods[i].gpc_addr0.s_addr == in_addr1.s_addr)
				return;
		}
	}

	for (i = 0; i < 8; i++) {
		if (gpcp->gpc_goods[i].gpc_flags == 0) {
			memcpy(gpcp->gpc_goods[i].gpc_ident, node, 20);
			gpcp->gpc_goods[i].gpc_port0 = in_port1;
			gpcp->gpc_goods[i].gpc_addr0 = in_addr1;
		   	gpcp->gpc_goods[i].gpc_flags = 1;
			return;
		}
	}

	memcpy(bad_ident, node, 20);
	for (i = 0; i < 8; i++) {
		if (ident_less(gpcp->gpc_target,
				   	bad_ident, gpcp->gpc_goods[i].gpc_ident)) {
			memcpy(bad_ident, gpcp->gpc_goods[i].gpc_ident, 20);
			index = i;
		}
	}

	if (index != -1) {
	   	memcpy(gpcp->gpc_goods[index].gpc_ident, node, 20);
	   	gpcp->gpc_goods[index].gpc_port0 = in_port1;
	   	gpcp->gpc_goods[index].gpc_addr0 = in_addr1;
	   	gpcp->gpc_goods[index].gpc_flags = 1;
	   	return;
	}
}

static void get_peers_insert(struct get_peer_context *gpcp,
	   	const char *node, in_addr in_addr1, u_short in_port1)
{
	int i;
	int less_count = 0;
	struct one_peer_upp * oupp;

	for (i = 0; i < MAX_PEER_COUNT; i++) {
		oupp = &gpcp->gpc_nodes[i];
		if (oupp->gpc_flags != 0) {
			if (oupp->gpc_port0 == in_port1 &&
				   	oupp->gpc_addr0.s_addr == in_addr1.s_addr)
				return;
			if (!memcmp(oupp->gpc_ident, node, 20))
				return;
		}

		if (oupp->gpc_flags == 2 &&
				ident_less(gpcp->gpc_target, oupp->gpc_ident, node)) {
			if (++less_count >= 8)
				return;
		}
	}

	for (i = 0; i < MAX_PEER_COUNT; i++) {
		oupp = &gpcp->gpc_nodes[i];
		if (oupp->gpc_flags == 0) {
			oupp->gpc_flags = 1;
			oupp->gpc_count = 0;
			oupp->gpc_touch = 0;
			oupp->gpc_addr0 = in_addr1;
			oupp->gpc_port0 = in_port1;
			memcpy(oupp->gpc_ident, node, 20);
		   	gpcp->gpc_total++;
			return;
		}
	}

	fprintf(stderr, "warn: too small buffer!\n");
	return;
}

static void kad_getpeers_input(void * upp)
{
	int i;
	size_t elen;
	btcodec codec;
	in_addr in_addr1;
	u_short in_port1;
	const char *node, *nodes;
	struct get_peer_context * gpcp;

	gpcp = (struct get_peer_context *)upp;

	for (i = 0; i < MAX_PEER_COUNT; i++) {
		if (waitcb_completed(&gpcp->gpc_nodes[i].gpc_wait0)) {
			gpcp->gpc_acked++;
			gpcp->gpc_nodes[i].gpc_flags = 2;
			codec.parse(gpcp->gpc_nodes[i].gpc_response, 
					gpcp->gpc_nodes[i].gpc_wait0.wt_count);

			nodes = codec.bget().bget("r").bget("id").c_str(&elen);
			if (nodes == NULL) {
			   	waitcb_clear(&gpcp->gpc_nodes[i].gpc_wait0);
				continue;
			}
		   
			get_peers_bound(gpcp, nodes, in_addr1, in_port1);
			nodes = codec.bget().bget("r").bget("nodes").c_str(&elen);
		   	if (nodes == NULL) {
			   	waitcb_clear(&gpcp->gpc_nodes[i].gpc_wait0);
				continue;
			}
		   
			for (node = nodes; elen >= 26; node += 26, elen -= 26) {
			   	memcpy(&in_addr1, node + 20, sizeof(in_addr1));
			   	memcpy(&in_port1, node + 24, sizeof(in_port1));
			   	get_peers_insert(gpcp, node, in_addr1, in_port1);
		   	}

			waitcb_clear(&gpcp->gpc_nodes[i].gpc_wait0);
		}
	}

	kad_getpeers_output(gpcp);
	return;
}

int kad_getpeers(const char * ident, const char * server)
{
	int i;
	int error;
	struct sockaddr_in so_addr;
	struct get_peer_context * gpcp;

	error = getaddrbyname(server, &so_addr);
	assert(error == 0);

	gpcp = new struct get_peer_context;
	gpcp->gpc_acked = 0;
	gpcp->gpc_total = 1;
	gpcp->gpc_sentout = 0;
	memcpy(gpcp->gpc_target, ident, 20);

	for (i = 1; i < 8; i++)
	   	gpcp->gpc_goods[i].gpc_flags = 0;

	for (i = 1; i < MAX_PEER_COUNT; i++) {
	   	gpcp->gpc_nodes[i].gpc_flags = 0;
	   	gpcp->gpc_nodes[i].gpc_count = 0;
	   	gpcp->gpc_nodes[i].gpc_touch = 0;
		waitcb_init(&gpcp->gpc_nodes[i].gpc_wait0, kad_getpeers_input, gpcp);
	}

	gpcp->gpc_nodes[0].gpc_flags = 1;
	gpcp->gpc_nodes[0].gpc_count = 0;
	gpcp->gpc_nodes[0].gpc_touch = 0;
	gpcp->gpc_nodes[0].gpc_addr0 = so_addr.sin_addr;
	gpcp->gpc_nodes[0].gpc_port0 = so_addr.sin_port;
   	waitcb_init(&gpcp->gpc_nodes[0].gpc_wait0, kad_getpeers_input, gpcp);
   	waitcb_init(&gpcp->gpc_timeout, kad_getpeers_output, gpcp);

	kad_getpeers_output(gpcp);
	return 0;
}

int kad_pingnode(const char * server)
{
   	int len;
	int error;
	char buf[2048];
	struct sockaddr_in so_addr;

	error = getaddrbyname(server, &so_addr);
	assert(error == 0);

	len = kad_ping_node(buf, sizeof(buf), ++_idgen);
	len = kad_auto_send(buf, len, &so_addr);
	return 0;
}

int kad_findnode(const char * ident, const char * server)
{
   	int len;
	int error;
	char buf[2048];
	struct sockaddr_in so_addr;

	error = getaddrbyname(server, &so_addr);
	assert(error == 0);

	len = kad_find_node(buf, sizeof(buf), ++_idgen, (const uint8_t *)ident);
	len = kad_auto_send(buf, len, &so_addr);
	return 0;
}

static void udp_daemon_control(void * upp)
{
	int error;
	struct sockaddr * so_addrp;
	struct sockaddr_in addr_in1;

	if (upp == &_startcb) {
		fprintf(stderr, "udp daemon start\n");
		_udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		assert(_udp_sockfd != -1);
		sock_created(_udp_sockfd);

		so_addrp = (struct sockaddr *)&addr_in1;
		addr_in1.sin_family = AF_INET;
		addr_in1.sin_port   = htons(8081);
		addr_in1.sin_addr.s_addr = htonl(INADDR_ANY);

		error = bind(_udp_sockfd, so_addrp, sizeof(addr_in1));
		assert(error == 0);
		
		waitcb_init(&_sockcb, udp_routine, 0);
		udp_routine(NULL);

		waitcb_init(&_timer0, udp_timer, 0);
		udp_timer(NULL);
		return;
	}

	if (upp == &_stopcb) {
		fprintf(stderr, "udp daemon stop\n");
		closesocket(_udp_sockfd);
		sock_closed(_udp_sockfd);
		waitcb_clean(&_sockcb);
		return;
	}

	return;
}

static void udp_daemon_init(void)
{
	_log_file = fopen("C:\\kad_log.log", "a");
	_dmp_file = fopen("C:\\kad_log.dat", "ab");
	waitcb_init(&_startcb, udp_daemon_control, &_startcb);
	slotwait_atstart(&_startcb);

	waitcb_init(&_stopcb, udp_daemon_control, &_stopcb);
	slotwait_atstop(&_stopcb);

	fprintf(stderr, "Hello\n");
	return;
}

static void udp_daemon_clean(void)
{
	fprintf(stderr, "World\n");
	waitcb_clean(&_startcb);
	waitcb_clean(&_stopcb);
	fclose(_log_file);
	fclose(_dmp_file);
	return;
}

struct module_stub udp_daemon_mod = {
	udp_daemon_init, udp_daemon_clean
};

