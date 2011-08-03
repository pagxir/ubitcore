#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <winsock.h>

#include "utils.h"
#include "module.h"
#include "callout.h"
#include "btcodec.h"
#include "slotwait.h"
#include "slotsock.h"
#include "kad_proto.h"
#include "kad_route.h"
#include "recursive.h"
#include "udp_daemon.h"

static FILE *_dmp_file;
static FILE *_log_file;
static int _udp_sockfd = 0;
static struct sockcb *_udp_sockcbp = 0;

static struct waitcb _sockcb;
static struct waitcb _stopcb;
static struct waitcb _startcb;

static slotcb _kad_slot = 0;
static int _id_gen = 0x1982;
static void dump_peer_values(btcodec & codec)
{
	int i;
	size_t elen;
	u_short in_port1;
   	in_addr in_addr1;
	btfastvisit btfv;
	const char *value, *values;
   
	i = 0;
	values = btfv(&codec).bget("r").bget("values").bget(i++).c_str(&elen);
   	while (values != NULL) {
	   	for (value = values; elen >= 6; value += 6, elen -= 6) {
		   	memcpy(&in_addr1, value + 0, sizeof(in_addr1));
		   	memcpy(&in_port1, value + 4, sizeof(in_port1));
		   	fprintf(stderr, "value %s:%d\n",
				   	inet_ntoa(in_addr1), ntohs(in_port1));
		   	fprintf(_log_file, "value %s:%d\n",
				   	inet_ntoa(in_addr1), ntohs(in_port1));
	   	}
	   	values = btfv(&codec).bget("r").bget("values").bget(i++).c_str(&elen);
   	}

	return;
}

static void dump_kad_peer_ident(const char *peer_ident,
	   	struct sockaddr_in *inp)
{
	size_t elen;
	uint8_t *u_peer_ident;
	u_peer_ident = (uint8_t *)peer_ident;

	fprintf(stderr, "peer_ident: ");
	elen = 20;
	while (elen-- > 0)
		fprintf(stderr, "%02X", *u_peer_ident++);
	fprintf(stderr, " %s:%d\n",
		   	inet_ntoa(inp->sin_addr), htons(inp->sin_port));
}

int send_node_ping(struct kad_node *knp)
{
	int err, len;
	char sockbuf[1024];
   	struct sockaddr *so_addrp;
   	struct sockaddr_in in_addr1;

	so_addrp = (struct sockaddr *)&in_addr1;
	in_addr1.sin_family = AF_INET;
	in_addr1.sin_port = knp->kn_addr.kc_port;
	in_addr1.sin_addr = knp->kn_addr.kc_addr;

	len = kad_ping_node(sockbuf, sizeof(sockbuf), (uint32_t)++_id_gen);
   	err = sendto(_udp_sockfd, sockbuf, len, 0, so_addrp, sizeof(in_addr1));
	return 0;
}

int send_bucket_update(const char *node)
{
	char identstr[41];
	printf("send_bucket_update: %s\n",
		   hex_encode(identstr, node, IDENT_LEN));	
	kad_findnode(node);
	return 0;
}

static void dump_info_hash(const char *info_hash, size_t elen)
{
#if 1
	struct sockaddr_in in_addr0;
	in_addr0.sin_family = AF_INET;
	in_addr0.sin_port   = htons(8866);
	in_addr0.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sendto(_udp_sockfd, info_hash, elen, 0, 
			(struct sockaddr *)&in_addr0, sizeof(in_addr0));
#endif
	return;
}

static void kad_proto_input(char *buf, size_t len, struct sockaddr_in *in_addrp)
{
	int i;
	int err;
	int found;

   	void *idp;
	size_t elen;
	btcodec codec;
	btfastvisit btfv;
	char out_buf[2048];
	struct kad_node knode;
   	struct waitcb *waitcbp;

   	const char *info_hash;
	const char *node, *nodes;
	const char *value, *values;
	const char *peer_ident = 0;
	const char *query_type = 0;
	const char *query_ident = 0;

	codec.load(buf, len);
	const char *type = btfv(&codec).bget("y").c_str(&elen);
	if (type == NULL || elen != 1) {
		fprintf(stderr, "[packet missing query type] %d\n", elen);
		return;
	}

	switch (*type) {
		case 'r':
		   	peer_ident = btfv(&codec).bget("r").bget("id").c_str(&elen);
			if (peer_ident == NULL || elen != 20) {
			   	fprintf(stderr, "answer packet missing peer ident\n");
				return;
			}
		   
			query_ident = btfv(&codec).bget("t").c_str(&elen);
		   	if (query_ident == NULL || elen > sizeof(uint32_t)) {
			   	fprintf(stderr, "packet missing packet ident\n");
				fwrite(buf, len, 1, _log_file);
			   	return;
		   	}

			idp = 0;
			found = 0;
			memcpy(&idp, query_ident, elen);
			if (elen != 4) {
				printf("elen = %d\n", elen);
				printf("addr: %s:%d\n", inet_ntoa(in_addrp->sin_addr), htons(in_addrp->sin_port));
			}

		   	for (waitcbp = _kad_slot; waitcbp;
				   	waitcbp = waitcbp->wt_next) {
			   	if (!memcmp(waitcbp->wt_data, query_ident, elen)) {
				   	assert(len < waitcbp->wt_count);
				   	waitcbp->wt_count = len;
				   	memcpy(waitcbp->wt_data, buf, len);
				   	waitcb_cancel(waitcbp);
				   	waitcb_switch(waitcbp);
					found = 1;
				   	break;
				}
		   	}
		   
			knode.kn_type = 1; 
			knode.kn_addr.kc_addr = in_addrp->sin_addr;
			knode.kn_addr.kc_port = in_addrp->sin_port;
			memcpy(knode.kn_ident, peer_ident, IDENT_LEN);
			kad_node_good(&knode);

			//dump_kad_peer_ident(peer_ident, in_addrp);
			dump_peer_values(codec);
			break;

		case 'q':
		   	peer_ident = btfv(&codec).bget("a").bget("id").c_str(&elen);
			if (peer_ident == NULL || elen != 20) {
			   	fprintf(stderr, "query packet missing peer ident\n");
				return;
			}

			query_type = btfv(&codec).bget("q").c_str(&elen);
			if (query_type == NULL) {
			   	fprintf(stderr, "query packet missing query type\n");
				return;
			}

			if (elen == 13 && !strncmp(query_type, "announce_peer", 13)) {
			   	//fprintf(stderr, "announce_peer packet\n");
			} else if (elen == 9 && !strncmp(query_type, "find_node", 9)) {
				int siz;
				char buf[1024];
				struct sockaddr *so_addrp;
				btentity *entity = btfv(&codec).bget("t").bget();
				so_addrp = (struct sockaddr *)in_addrp;
				info_hash = btfv(&codec).bget("a").bget("target").c_str(&elen);
				siz = kad_krpc_closest(info_hash, buf, sizeof(buf));
				len = kad_find_node_answer(out_buf, sizeof(out_buf), entity, buf, siz);
				err = sendto(_udp_sockfd, out_buf, len,
					   	0, so_addrp, sizeof(*in_addrp));
			   	//fprintf(stderr, "find_node packet: %d\n", err);
			} else if (elen == 9 && !strncmp(query_type, "get_peers", 9)) {
				int siz;
				char buf[1024];
				struct sockaddr *so_addrp;
				btentity *entity = btfv(&codec).bget("t").bget();
				so_addrp = (struct sockaddr *)in_addrp;
				info_hash = btfv(&codec).bget("a").bget("info_hash").c_str(&elen);
				siz = kad_krpc_closest(info_hash, buf, sizeof(buf));
				len = kad_get_peers_answer(out_buf, sizeof(out_buf), entity, buf, siz);
				err = sendto(_udp_sockfd, out_buf, len,
					   	0, so_addrp, sizeof(*in_addrp));
				dump_info_hash(info_hash, elen);
			} else if (elen == 4 && !strncmp(query_type, "ping", 4)) {
				struct sockaddr *so_addrp;
				btentity *entity = btfv(&codec).bget("t").bget();
				so_addrp = (struct sockaddr *)in_addrp;
				len = kad_ping_node_answer(out_buf, sizeof(out_buf), entity);
				err = sendto(_udp_sockfd, out_buf, len,
					   	0, so_addrp, sizeof(*in_addrp));
#if 0
			   	//fprintf(stderr, "ping packet\n");
			} else {
				//fprintf(stderr, "query packet have an unkown query type: %.8s\n", query_type);
				return;
#endif
			}

			knode.kn_type = 1; 
			knode.kn_addr.kc_addr = in_addrp->sin_addr;
			knode.kn_addr.kc_port = in_addrp->sin_port;
			memcpy(knode.kn_ident, peer_ident, IDENT_LEN);
			kad_node_seen(&knode);
			//dump_kad_peer_ident(peer_ident, in_addrp);
			break;

		default:
			if (*type == 'e') {
			   	fprintf(stderr, "peer return error\n");
				break;
			}
			break;
	}

	return;
}

static void udp_routine(void *upp)
{
	int count, sockfd;
	char sockbuf[2048];
	struct sockaddr_in in_addr1;

	sockfd = _udp_sockfd;
	waitcb_cancel(&_sockcb);
	for ( ; ; ) {
		int in_len1 = sizeof(in_addr1);
	   	count = recvfrom(sockfd, sockbuf, sizeof(sockbuf),
			   	0, (struct sockaddr *)&in_addr1, &in_len1);
		if (count >= 0) {
			kad_proto_input(sockbuf, count, &in_addr1);
			fwrite(sockbuf, count, 1, _dmp_file);
		} else if (WSAGetLastError() == WSAEWOULDBLOCK) {
			sock_read_wait(_udp_sockcbp, &_sockcb);
			break;
		} else if (count == -1) {
#if 0
			fprintf(stderr, "recv error %d from %s:%d\n",
				   	GetLastError(), inet_ntoa(in_addr1.sin_addr), ntohs(in_addr1.sin_port));
#endif
			continue;
		}
	}

	return;
}

/*
 * router.utorrent.com
 * router.bittorrent.com:6881
 */
int kad_setup(const char *ident)
{
	kad_set_ident(ident);
	return 0;
}

int kad_pingnode(const char *server)
{
    int error;
	struct kad_node knode;
    struct sockaddr_in so_addr;

    error = getaddrbyname(server, &so_addr);
    if (error != 0) {
        fprintf(stderr, "getaddrbyname failure\n");
        return 0;
    }

	knode.kn_addr.kc_addr = so_addr.sin_addr;
	knode.kn_addr.kc_port = so_addr.sin_port;

	return send_node_ping(&knode);
}

int kad_getpeers(const char *ident)
{
	kad_recursive(RC_TYPE_GET_PEERS, ident);
	return 0;
}

int kad_findnode(const char *ident)
{
	kad_recursive(RC_TYPE_FIND_NODE, ident);
	return 0;
}

int kad_proto_out(int type, const char *target,
		const struct sockaddr_in *soap, char *outp,
	   	size_t size, struct waitcb *waitcbp)
{
	int len;
	int error;
	char sockbuf[2048];

	switch (type) {
		case RC_TYPE_FIND_NODE:
			len = kad_find_node(sockbuf, sizeof(sockbuf),
				   	(uint32_t)++_id_gen, (uint8_t *)target);
			break;

		case RC_TYPE_GET_PEERS:
			len = kad_get_peers(sockbuf, sizeof(sockbuf),
				   	(uint32_t)++_id_gen, (uint8_t *)target);
			break;

		case RC_TYPE_PING_NODE:
			printf("ping_node: %s:%d\n",
				   	inet_ntoa(soap->sin_addr), htons(soap->sin_port));
			len = kad_ping_node(sockbuf, sizeof(sockbuf), (uint32_t)++_id_gen);
			break;

		default:
			assert(0);
			break;
	}

	error = sendto(_udp_sockfd, sockbuf, len, 0,
			(const struct sockaddr *)soap, sizeof(*soap));

	if (error != len)
		return -1;

	assert(outp != NULL);
	assert(sizeof(_id_gen) < size);
	memcpy(outp, &_id_gen, sizeof(_id_gen));

	waitcbp->wt_data = outp;
   	waitcbp->wt_count = size;
   	slot_record(&_kad_slot, waitcbp);

	return 0;
}

static void udp_daemon_control(void *upp)
{
	int i;
	int error;
	int ports[2] = {8081, 8082};
	struct sockaddr *so_addrp;
	struct sockaddr_in addr_in1;

	if (upp == &_startcb) {
		fprintf(stderr, "udp daemon start\n");
		_udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		assert(_udp_sockfd != -1);
		_udp_sockcbp = sock_attach(_udp_sockfd);

		so_addrp = (struct sockaddr *)&addr_in1;
	   	addr_in1.sin_family = AF_INET;
	   	addr_in1.sin_addr.s_addr = htonl(INADDR_ANY);

		i = 0;
		do {
			assert(i < 2);
		   	addr_in1.sin_port   = htons(ports[i++]);
		   	error = bind(_udp_sockfd, so_addrp, sizeof(addr_in1));
		} while (error != 0);
		
		waitcb_init(&_sockcb, udp_routine, 0);
		udp_routine(NULL);
		return;
	}

	if (upp == &_stopcb) {
		fprintf(stderr, "udp daemon stop\n");
		sock_detach(_udp_sockcbp);
		closesocket(_udp_sockfd);
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

	return;
}

static void udp_daemon_clean(void)
{
	waitcb_clean(&_startcb);
	waitcb_clean(&_stopcb);
	fclose(_log_file);
	fclose(_dmp_file);
	return;
}

struct module_stub udp_daemon_mod = {
	udp_daemon_init, udp_daemon_clean
};

