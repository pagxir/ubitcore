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
#include "recursive.h"
#include "udp_daemon.h"

static FILE * _dmp_file;
static FILE * _log_file;
static int _udp_sockfd = 0;
static struct waitcb _sockcb;
static struct waitcb _stopcb;
static struct waitcb _startcb;

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
			if (waitcbp != idp)
				continue;
		   	assert(len < waitcbp->wt_count);
		   	waitcbp->wt_count = len;
		   	memcpy(waitcbp->wt_data, buf, len);
		   	waitcb_cancel(waitcbp);
		   	waitcb_switch(waitcbp);
		   	break;
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

int kad_pingnode(const char * server)
{
	static char just_keep_it[20] = "just keep it";
	kad_recursive(RC_TYPE_PING_NODE, just_keep_it, server);
	return 0;
}

int kad_getpeers(const char * ident, const char * server)
{
	kad_recursive(RC_TYPE_GET_PEERS, ident, server);
	return 0;
}

int kad_findnode(const char * ident, const char * server)
{
	kad_recursive(RC_TYPE_FIND_NODE, ident, server);
	return 0;
}

int kad_proto_out(int type, const char * target,
		const struct sockaddr_in * soap, char * outp,
	   	size_t size, struct waitcb * waitcbp)
{
	int len;
	int error;
	char sockbuf[2048];

	switch (type) {
		case RC_TYPE_FIND_NODE:
			len = kad_find_node(sockbuf, sizeof(sockbuf),
				   	(uint32_t)waitcbp, (uint8_t *)target);
			break;

		case RC_TYPE_GET_PEERS:
			len = kad_get_peers(sockbuf, sizeof(sockbuf),
				   	(uint32_t)waitcbp, (uint8_t *)target);
			break;

		case RC_TYPE_PING_NODE:
			len = kad_ping_node(sockbuf, sizeof(sockbuf), (uint32_t)waitcbp);
			break;

		default:
			assert(0);
			break;
	}

	error = sendto(_udp_sockfd, sockbuf, len, 0,
			(const struct sockaddr *)soap, sizeof(*soap));

	if (error != len)
		return -1;

	waitcbp->wt_data = outp;
   	waitcbp->wt_count = size;
   	slot_insert(&_kad_slot, waitcbp);

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

