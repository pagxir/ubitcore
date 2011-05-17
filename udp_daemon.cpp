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

static struct waitcb _sockcb;
static struct waitcb _stopcb;
static struct waitcb _startcb;

static void hexdump(char * buf, size_t len)
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

static void kad_proto_input(char * buf, size_t len)
{
	size_t elen;
	btcodec codec;
	u_short in_port1;
   	in_addr in_addr1;
	const char *node, *nodes;

	codec.parse(buf, len);
	nodes = codec.bget().bget("r").bget("nodes").c_str(&elen);
	for (node = nodes; elen >= 26; node += 26, elen -= 26) {
		memcpy(&in_addr1, node + 20, sizeof(in_addr1));
		memcpy(&in_port1, node + 24, sizeof(in_port1));
		fprintf(stderr, "%s:%d\n", inet_ntoa(in_addr1), ntohs(in_port1));
	}

	return;
}

static int _udp_sockfd = 0;
static void udp_routine(void * upp)
{
	int count, sockfd;
	char sockbuf[1024];
	struct sockaddr_in in_addr1;

	sockfd = _udp_sockfd;
	waitcb_cancel(&_sockcb);
	for ( ; ; ) {
	   	count = recvfrom2(sockfd, sockbuf,
			   	sizeof(sockbuf), &in_addr1, &_sockcb);
		if (count >= 0) {
			kad_proto_input(sockbuf, count);
			hexdump(sockbuf, count);
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

static int _idgen = 0;
static uint8_t _kadid[20];
static struct waitcb _timer0;
/*
 * router.utorrent.com
 * router.bittorrent.com:6881
 */

static void udp_timer(void * upp)
{
	int len;
	char buf[2048];
	struct sockaddr_in so_addr;

	so_addr.sin_family = AF_INET;
	so_addr.sin_port   = htons(6881);
	so_addr.sin_addr.s_addr = inet_addr("67.215.242.138");

	len = kad_get_peers(buf, sizeof(buf), ++_idgen, _kadid);
	len = sendto2(_udp_sockfd, buf, len, &so_addr, &_timer0);
	callout_reset(&_timer0, 2000);
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
		addr_in1.sin_port   = htons(8080);
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
	return;
}

struct module_stub udp_daemon_mod = {
	udp_daemon_init, udp_daemon_clean
};

