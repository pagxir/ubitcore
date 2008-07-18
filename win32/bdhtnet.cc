/* $Id$ */
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <queue>
#include <stack>
#include <set>

#include "buinet.h"
#include "bthread.h"
#include "bsocket.h"
#include "bdhtnet.h"
#include "btcodec.h"
#include "butils.h"

#define DHT_TTL 3
#define PF_PING  0x1000
#define PF_FIND  0x2000
#define PF_PEER  0x0100
#define PF_QPEER  0x0200
#define PF_QFIND  0x4000
#define PF_QPING  0x8000

#if 0
#define get_peer_ident() __dht_id
extern unsigned char __dht_id[];
#endif

unsigned char __ping_nodes[] = {
	"d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t1:P1:y1:qe"
};

unsigned char __find_nodes[] = {
    "d1:ad2:id20:abcdefghij01234567896:target20:abcdefghij0123456789e1:q9:find_node1:t1:F1:y1:qe"
};

unsigned char __getpeers[] = {
    "d1:ad2:id20:abcdefghij01234567899:info_hash20:mnopqrstuvwxyz123456e1:q9:get_peers1:t1:G1:y1:qe"
};


struct bdhtnode{
	int            b_life;
	unsigned short b_flag;
	unsigned short b_port;
	unsigned long  b_host;
	unsigned char  b_ident[20];
	time_t         b_last_find;
	time_t         b_last_update;
};

bool operator < (const bdhtnode& l, const bdhtnode &r)
{
	int i;
    const unsigned char *selfid = get_peer_ident();
	for (i=0; i<20; i++){
         unsigned char lident = selfid[i]^l.b_ident[i];
         unsigned char rident = selfid[i]^r.b_ident[i];
		if (lident < rident){
			return true;
		}
		if (lident > rident){
			return false;
		}
	}
	return false; 
}

struct peers_lesser{
	bool operator()(const bdhtnode *l, const bdhtnode *r)
	{
	   	int i;
        const unsigned char *info_hash = get_info_hash();
	   	for (i=0; i<20; i++){
		   	unsigned char lident = info_hash[i]^l->b_ident[i];
		   	unsigned char rident = info_hash[i]^r->b_ident[i];
		   	if (lident < rident){
			   	return true;
		   	}
		   	if (lident > rident){
			   	return false;
		   	}
	   	}
	   	return false; 
	}
};

struct ident_lesser{
	bool operator()(const bdhtnode *l, const bdhtnode *r)
	{
		return *l<*r;
	}
};

struct socket_lesser{
	bool operator()(const bdhtnode *l, const bdhtnode *r)
	{
		if (l->b_host != r->b_host){
			return l->b_host<r->b_host;
		}
		return l->b_port<r->b_port;
	}
};

static std::queue<bdhtnode*> __out_nodes;
static std::set<bdhtnode*, socket_lesser> __world_nodes;
static std::set<bdhtnode*, socket_lesser> __ident_nodes;
static std::set<bdhtnode*, ident_lesser>  __stack_find;
static std::set<bdhtnode*, peers_lesser>  __stack_getpeers;

class bdhtnet
{
    public:
        bdhtnet();
        int bplay(time_t timeout);
        int brecord(time_t timeout);

    private:
        bsocket b_socket;
        time_t b_last;
};

typedef int (bdhtnet::*p_bcall)(time_t timeout);

static bdhtnet __dhtnet;

bdhtnet::bdhtnet()
{
}

class bdhtnet_wrapper: public bthread
{
    public:
        bdhtnet_wrapper(p_bcall call);
        virtual int bdocall(time_t timeout);

    private:
        p_bcall b_call;
};

static bdhtnet_wrapper __play(&bdhtnet::bplay);
static bdhtnet_wrapper __record(&bdhtnet::brecord);

bdhtnet_wrapper::bdhtnet_wrapper(p_bcall call):
    b_call(call)
{
}

int bdhtnet_wrapper::bdocall(time_t timeout)
{
    return (__dhtnet.*b_call)(timeout);
}

static bdhtnode *__last_good_getpeer = NULL;
static int
active_get_peers(bdhtnode *node)
{
	peers_lesser lesser;
	if (__last_good_getpeer == NULL){
		__stack_getpeers.insert(node);
		__play.bwakeup();
	}else if (lesser.operator()(node, __last_good_getpeer)){
		__stack_getpeers.insert(node);
		__play.bwakeup();
	}
	node->b_flag |= PF_QPEER;
	return 0;
}


static bdhtnode *__last_good_find = NULL;
static int
active_find_node(bdhtnode *node)
{
	if (__last_good_find == NULL){
		__stack_find.insert(node);
		__play.bwakeup();
	}else if (*node < *__last_good_find){
		__stack_find.insert(node);
		__play.bwakeup();
	}
	node->b_flag |= PF_QFIND;
	return 0;
}

static int
bdht_build(unsigned long host, unsigned short port,
        const char id[], size_t elen)
{
	bdhtnode tnode, *build_node;
	assert(elen==20);
	tnode.b_life = DHT_TTL;
	tnode.b_port = port;
	tnode.b_host = host;
	tnode.b_flag = PF_PING|PF_FIND|PF_PEER;
	memcpy(tnode.b_ident, id, 20);
	if (__world_nodes.find(&tnode) == __world_nodes.end()){
		build_node = new bdhtnode(tnode);
		__world_nodes.insert(build_node);
	}else{
		build_node = *__world_nodes.find(&tnode);
	}
	if (build_node->b_flag&PF_FIND){
		if (0==(build_node->b_flag&PF_QFIND)){
		   	active_find_node(build_node);
		}
	}
	if (build_node->b_flag&PF_PEER){
		if (0==(build_node->b_flag&PF_QPEER)){
			active_get_peers(build_node);
		}
	}
	if (build_node->b_flag&PF_PING){
		if (0==(build_node->b_flag&PF_QPING)){
		   	__out_nodes.push(build_node);
			build_node->b_flag |= PF_QPING;
		}
	}
    return 0;
}

static int
bdht_decode_nodes(const char *nodes, int eln)
{
    int i;
    unsigned long host;
    unsigned short port;
    for (i=0; i+26<=eln; i+=26){
        memcpy(&host, nodes+i+20, 4);
        memcpy(&port, nodes+i+24, 2);
        port = htons(port);
        bdht_build(host, port, nodes+i, 20);
#if 0
        printf("add nodes: %s:%d\n",
                inet_ntoa(*(in_addr*)&host), htons(port));
#endif
    }
    assert(eln==i);
    return 0;
}

static int
bdht_decode_peers(const char *values, int eln)
{
	printf("----------------------recv peers---------------------\n");
    return 0;
}

static int
dump_find_node(bdhtnode *node, const char *title="find:")
{
	int i;

	printf("%s ", title);
	for (i=0; i<20; i++){
		printf("%02x", node->b_ident[i]&0xff);
	}
	printf(" %s:%d %d\n", inet_ntoa(*(in_addr*)&node->b_host), node->b_port, node->b_life);
	return 0;
}

int
bdhtnet::brecord(time_t timeout)
{
    int error = 0;
    char buffer[8192];
	bdhtnode tnode;
    unsigned long host;
    unsigned short port;
    while(error != -1){
        error = b_socket.brecvfrom(buffer, sizeof(buffer), &host, &port);
        if (error >= 0){
            size_t elen;
            btcodec codec;
			tnode.b_port = port;
			tnode.b_host = host;
            codec.bload(buffer, error);

            const char *p1 = codec.bget().bget("y").c_str(&elen);
            const char *t2 = codec.bget().bget("t").c_str(&elen);

            printf("R: %c %c %s:%d\n", p1?*p1:'@', t2?*t2:'!',
                    inet_ntoa(*(in_addr*)&host), port);

			if (p1!=NULL && *p1!='r'){
				continue;
			}

            const char *ident = codec.bget().bget("r").bget("id").c_str(&elen);

			assert(elen == 20);
			assert(ident != NULL);
			if (__ident_nodes.find(&tnode) != __ident_nodes.end()){
				bdhtnode *id_node = *__ident_nodes.find(&tnode);
				__ident_nodes.erase(id_node);
				memcpy(id_node->b_ident, ident, 20);
				assert(__world_nodes.find(id_node)
						== __world_nodes.end());
				__world_nodes.insert(id_node);
				id_node->b_flag |= PF_PEER;
				id_node->b_flag |= PF_FIND;
				id_node->b_flag &= ~PF_PING;
				id_node->b_life = DHT_TTL;
				if ((id_node->b_flag&PF_QFIND)==0){
				   	active_find_node(id_node);
				}
				if ((id_node->b_flag&PF_QPEER)==0){
				   	active_get_peers(id_node);
				}
			}else if (p1!=NULL && *p1=='r'){
			    std::set<bdhtnode*, socket_lesser>::iterator nfind;
			    nfind = __world_nodes.find(&tnode);
				assert(nfind != __world_nodes.end());

#if 1
			   	const char *peers = codec.bget().bget("r").bget("values").c_str(&elen);
			   	if (peers != NULL){
				   	bdht_decode_peers(peers, elen);
				}
#endif
			   	const char *nodes = codec.bget().bget("r").bget("nodes").c_str(&elen);
			   	if (nodes != NULL){
					bdht_decode_nodes(nodes, elen);
			   	}

			   	const char *token = codec.bget().bget("r").bget("token").c_str(&elen);
				if (token != NULL){
					printf("get token!\n");
					peers_lesser lesser;
					if (__last_good_getpeer == NULL){
					   	__last_good_getpeer = *nfind;
					}else if (lesser.operator()(*nfind, __last_good_getpeer)){
					   	__last_good_getpeer = *nfind;
					}
				   	(*nfind)->b_flag &= ~PF_PEER;
				}else if (nodes != NULL){
					if (__last_good_find == NULL){
						__last_good_find = *nfind;
					}else if (**nfind < *__last_good_find){
						__last_good_find = *nfind;
					}
					__last_good_find = *nfind;
				   	(*nfind)->b_flag &= ~PF_FIND;
				}

			   	dump_find_node(*nfind, "recved:");
			   	(*nfind)->b_flag &= ~PF_PING;
				(*nfind)->b_life = DHT_TTL;
			}
        }
    }
    return error;
}

int
bdhtnet::bplay(time_t timeout)
{
    int error = 0;
    int count = 0;
    bdhtnode *marker = NULL;
    unsigned long host = 0;
    unsigned short port = 0;
	bdhtnode *find_node = NULL;
	bdhtnode *get_peer_node = NULL;
	while (!__stack_find.empty() && count<5){
		std::set<bdhtnode*, ident_lesser>::iterator start;
		start = __stack_find.begin();
        if (!((*start)->b_flag&PF_FIND)){
			__stack_find.clear();
			break;
		}
		if ((*start)->b_life>0){
			find_node = *start;
		   	error = b_socket.bsendto(__find_nodes, sizeof(__find_nodes)-1,
					(*start)->b_host, (*start)->b_port);
			find_node->b_life--;
			count ++;
			dump_find_node(*start);
			break;
		}
		__stack_find.erase(start);
	}
	while (!__stack_getpeers.empty() && count<5){
		std::set<bdhtnode*, peers_lesser>::iterator start;
		start = __stack_getpeers.begin();
        if (!((*start)->b_flag&PF_PEER)){
			__stack_getpeers.clear();
			break;
		}
		if ((*start)->b_life>=0){
			get_peer_node = *start;
		   	error = b_socket.bsendto(__getpeers, sizeof(__getpeers)-1,
					(*start)->b_host, (*start)->b_port);
			if (get_peer_node != find_node){
			   	get_peer_node->b_life--;
			}
			count ++;
			dump_find_node(*start, "getpeers:");
			break;
		}
		__stack_getpeers.erase(start);
	}
    while (!__out_nodes.empty() && count<5){
        bdhtnode *p = __out_nodes.front();
        if (p == marker){
            break;
        }
        __out_nodes.pop();
	   	if (find_node==p || get_peer_node==p){
			//printf("skip find node\n");
	   	}else if (p->b_life == 0){
			//printf("node is dead!\n");
			continue;
		}else if (p->b_flag&PF_PING){
			p->b_life--;
		   	error = b_socket.bsendto(__ping_nodes,
				   	sizeof(__ping_nodes)-1, p->b_host, p->b_port);
		   	assert(error != -1);
		   	count ++;
		}else {
			//printf("node remove from play list!\n");
			continue;
		}
		if (marker == NULL){
			marker = p;
		}
		__out_nodes.push(p);
    }
    if (error != -1 && count!=0){
        error = btime_wait(time(NULL)+1);
    }else{
        printf("finish!\n");
    }
    return error;
}

int
bdhtnet_node(const char *host, int port)
{
	static int __call_once_only = 0;
	if (__call_once_only++ == 0){
		memcpy(__getpeers+12, get_peer_ident(), 20);
		memcpy(__getpeers+46, get_info_hash(), 20);
		memcpy(__ping_nodes+12, get_peer_ident(), 20);
		memcpy(__find_nodes+12, get_peer_ident(), 20);
		memcpy(__find_nodes+43, get_peer_ident(), 20);
		__find_nodes[62]^=0x1;
	}

	bdhtnode *node = new bdhtnode();
	node->b_life = DHT_TTL;
	node->b_flag = PF_PING|PF_QPING;
	node->b_host = inet_addr(host);
	node->b_port = port;

	__out_nodes.push(node);
    __ident_nodes.insert(node);

    __play.bwakeup();
    __record.bwakeup();
    return 0;
}
