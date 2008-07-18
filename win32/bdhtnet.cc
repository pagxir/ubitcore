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

#define PF_PING  0x1000
#define PF_FIND  0x2000

unsigned char __ping_nodes[] = {
  0x64, 0x31, 0x3a, 0x61, 0x64, 0x32, 0x3a, 0x69, 0x64, 0x32, 0x30, 0x3a,
  0xd3, 0x86, 0x4b, 0x67, 0x3f, 0xc9, 0x47, 0xe1, 0xa8, 0xd8, 0x55, 0xd7,
  0x00, 0x32, 0xbc, 0x44, 0x48, 0x56, 0x23, 0xaa, 0x65, 0x31, 0x3a, 0x71,
  0x34, 0x3a, 0x70, 0x69, 0x6e, 0x67, 0x31, 0x3a, 0x74, 0x38, 0x3a, 0x51,
  0x3b, 0x1b, 0x00, 0x01, 0x00, 0x00, 0x00, 0x31, 0x3a, 0x79, 0x31, 0x3a,
  0x71, 0x65
};

unsigned char __find_nodes[] = {
  0x64, 0x31, 0x3a, 0x61, 0x64, 0x32, 0x3a, 0x69, 0x64, 0x32, 0x30, 0x3a,
  0xd3, 0x86, 0x4b, 0x67, 0x3f, 0xc9, 0x47, 0xe1, 0xa8, 0xd8, 0x55, 0xd7,
  0x00, 0x32, 0xbc, 0x44, 0x48, 0x56, 0x23, 0xaa, 0x36, 0x3a, 0x74, 0x61,
  0x72, 0x67, 0x65, 0x74, 0x32, 0x30, 0x3a, 0xd3, 0x86, 0x48, 0x67, 0x3f,
  0xc9, 0x47, 0xe1, 0xa8, 0xd8, 0x55, 0xd7, 0x00, 0x32, 0xbc, 0x44, 0x48,
  0x56, 0x23, 0xab, 0x65, 0x31, 0x3a, 0x71, 0x39, 0x3a, 0x66, 0x69, 0x6e,
  0x64, 0x5f, 0x6e, 0x6f, 0x64, 0x65, 0x31, 0x3a, 0x74, 0x38, 0x3a, 0x79,
  0x3b, 0x1b, 0x00, 0x01, 0x00, 0x00, 0x00, 0x31, 0x3a, 0x79, 0x31, 0x3a,
  0x71, 0x65
};

static unsigned char __selfid[20];

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
	for (i=0; i<20; i++){
         unsigned char lident = __selfid[i]^l.b_ident[i];
         unsigned char rident = __selfid[i]^r.b_ident[i];
		if (lident < rident){
			return true;
		}
		if (lident > rident){
			return false;
		}
	}
	return false; 
}

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
	return 0;
}

static int
bdht_build(unsigned long host, unsigned short port,
        const char id[], size_t elen)
{
	bdhtnode tnode, *build_node;
	assert(elen==20);
	tnode.b_life = 3;
	tnode.b_port = port;
	tnode.b_host = host;
	tnode.b_flag = PF_PING|PF_FIND;
	memcpy(tnode.b_ident, id, 20);
	if (__world_nodes.find(&tnode) == __world_nodes.end()){
		build_node = new bdhtnode(tnode);
		__world_nodes.insert(build_node);
	}else{
		build_node = *__world_nodes.find(&tnode);
	}
	if (build_node->b_flag&PF_FIND){
	   	active_find_node(build_node);
	}
	if (build_node->b_flag&PF_PING){
		__out_nodes.push(build_node);
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
        printf("add nodes: %s:%d\n",
                inet_ntoa(*(in_addr*)&host), htons(port));
    }
    assert(eln==i);
    return 0;
}

static int
bdht_decode_peers(const char *values, int eln)
{
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
				id_node->b_flag |= PF_FIND;
				id_node->b_flag &= ~PF_PING;
				id_node->b_life = 3;
				active_find_node(id_node);
			}else if (p1!=NULL && *p1=='r'){
			    std::set<bdhtnode*, socket_lesser>::iterator nfind;
			    nfind = __world_nodes.find(&tnode);
				assert(nfind != __world_nodes.end());

#if 0
			   	const char *peers = codec.bget().bget("r").bget("values").c_str(&elen);
			   	if (peers != NULL){
				   	bdht_decode_peers(peers, elen);
				   	bdht_build(host, port, ident, 20);
				}
#endif
			   	const char *nodes = codec.bget().bget("r").bget("nodes").c_str(&elen);
			   	if (nodes != NULL){
					bdht_decode_nodes(nodes, elen);
					(*nfind)->b_flag &= ~PF_FIND;
					__last_good_find = *nfind;
			   	}
			   	(*nfind)->b_flag &= ~PF_PING;
				(*nfind)->b_life = 3;
			}
        }
    }
    return error;
}

static int
dump_find_node(bdhtnode *node)
{
	int i;
	for (i=0; i<20; i++){
		printf("%02x", __selfid[i]&0xff);
	}
	printf("\n");
	printf("find: ");
	for (i=0; i<20; i++){
		printf("%02x", node->b_ident[i]&0xff);
	}
	printf(" %s:%d %d\n", inet_ntoa(*(in_addr*)&node->b_host), node->b_port, node->b_life);
	return 0;
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
	while (!__stack_find.empty() && count<5){
		std::set<bdhtnode*, ident_lesser>::iterator start;
		start = __stack_find.begin();
        if (!((*start)->b_flag&PF_FIND)){
			__stack_find.clear();
			break;
		}
		if ((*start)->b_life>0){
			find_node = *start;
		   	error = b_socket.bsendto(__find_nodes, sizeof(__find_nodes),
					(*start)->b_host, (*start)->b_port);
			find_node->b_life--;
			count ++;
			dump_find_node(*start);
			break;
		}
		__stack_find.erase(start);
	}
    while (!__out_nodes.empty() && count<5){
        bdhtnode *p = __out_nodes.front();
        if (p == marker){
            break;
        }
        __out_nodes.pop();
	   	if (find_node==p){
			printf("skip find node\n");
	   	}else if (p->b_life == 0){
			printf("node is dead!\n");
			continue;
		}else if (p->b_flag&PF_PING){
			p->b_life--;
		   	error = b_socket.bsendto(__ping_nodes,
				   	sizeof(__ping_nodes), p->b_host, p->b_port);
		   	assert(error != -1);
		   	count ++;
		}else {
			printf("node remove from play list!\n");
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
	   	btcodec codec;
	   	size_t idl;
	   	codec.bload((char*)__ping_nodes, sizeof(__ping_nodes));
	   	const char *ident = codec.bget().bget("a").bget("id").c_str(&idl);
		assert(idl == 20 && ident!=NULL);
		memcpy(__selfid, ident, 20);
	}

	bdhtnode *node = new bdhtnode();
	node->b_life = 3;
	node->b_flag = PF_PING;
	node->b_host = inet_addr(host);
	node->b_port = port;

	__out_nodes.push(node);
    __ident_nodes.insert(node);

    __play.bwakeup();
    __record.bwakeup();
    return 0;
}
