/* $Id$ */
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <queue>

#include "buinet.h"
#include "bthread.h"
#include "bsocket.h"
#include "bdhtnet.h"
#include "btcodec.h"

struct d_peer{
    int b_len;
    char *b_packet;
	unsigned short b_flag;
	unsigned short b_port;
	unsigned long b_host;
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

static d_peer __bluck[160][8];

std::queue<d_peer*> __q_peers;
std::queue<d_peer*> __q_wait;
std::queue<d_peer*> __q_recall;

class bdhtnet
{
    public:
        bdhtnet();
        int bplay(time_t timeout);
        int brecord(time_t timeout);

    private:
        bsocket b_socket;
        int b_state;
		int b_count;
        time_t b_last;
};

typedef int (bdhtnet::*p_bcall)(time_t timeout);

static bdhtnet __dhtnet;

bdhtnet::bdhtnet():
    b_count(0), b_state(0)
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

unsigned char __dht_ping[] = {
  0x64, 0x31, 0x3a, 0x61, 0x64, 0x32, 0x3a, 0x69, 0x64, 0x32, 0x30, 0x3a,
  0xd3, 0x86, 0x4b, 0x67, 0x3f, 0xc9, 0x47, 0xe1, 0xa8, 0xd8, 0x55, 0xd7,
  0x00, 0x32, 0xbc, 0x44, 0x48, 0x56, 0x23, 0xaa, 0x65, 0x31, 0x3a, 0x71,
  0x34, 0x3a, 0x70, 0x69, 0x6e, 0x67, 0x31, 0x3a, 0x74, 0x38, 0x3a, 0x51,
  0x3b, 0x1b, 0x00, 0x01, 0x00, 0x00, 0x00, 0x31, 0x3a, 0x79, 0x31, 0x3a,
  0x71, 0x65
};

static d_peer __peer;
static unsigned char __peer_id[20];

static int
bdht_build(unsigned long host, unsigned short port,
        const char id[], size_t elen)
{
    int i;
    assert(elen==20);
    const unsigned char *bundle = (const unsigned char*)id;
    printf("peer id: ");
    for (i=0; i<elen; i++){
        printf("%02x", bundle[i]);
    }
    printf("\n");
    btcodec codec;
    size_t idl;
    codec.bload((char*)__dht_ping, sizeof(__dht_ping));
    const unsigned char *s_id = (const unsigned char*)
        codec.bget().bget("a").bget("id").c_str(&idl);
    assert(idl==20);
    for (i=0; i<20; i++){
        int diff = (s_id[i]^bundle[i])-(s_id[i]^__peer_id[i]);
        if (diff == 0){
            continue;
        }
        if (diff < 0){
            memcpy(__peer_id, bundle, 20);
            __peer.b_host = host;
            __peer.b_port = port;
            __peer.b_len = sizeof(__find_nodes);
            __peer.b_packet = (char*)__find_nodes;
            if (__peer.b_flag == 0){
                __q_peers.push(&__peer);
            }
            __peer.b_flag = 3;
            __play.bwakeup();
            int j;
            printf("find node: ");
            for (j=0; j<elen; j++){
                printf("%02x", bundle[j]);
            }
            printf(" ");
            for (j=0; j<elen; j++){
                printf("%02x", s_id[j]);
            }
            printf("\n");
        }
        break;
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
    unsigned long host;
    unsigned short port;
    while(error != -1){
        error = b_socket.brecvfrom(buffer, sizeof(buffer), &host, &port);
        if (error >= 0){
            size_t elen;
            btcodec codec;
            codec.bload(buffer, error);
            const char *p1 = codec.bget().bget("y").c_str(&elen);
            const char *t2 = codec.bget().bget("t").c_str(&elen);
            printf("R: %c %c %s:%d\n", p1?*p1:'@', t2?*t2:'!',
                    inet_ntoa(*(in_addr*)&host), port);
            const char *id = codec.bget().bget("r").bget("id").c_str(&elen);
            if (id != NULL){
                assert(elen==20);
                bdht_build(host, port, id, elen);
            }
            const char *nodes = codec.bget().bget("r").bget("nodes").c_str(&elen);
            if (nodes != NULL){
                bdht_decode_nodes(nodes, elen);
            }
            const char *peers = codec.bget().bget("r").bget("values").c_str(&elen);
            if (nodes != NULL){
                bdht_decode_peers(peers, elen);
            }
            d_peer *marker = new d_peer();
            marker->b_flag = -1;
            __q_wait.push(marker);
            d_peer *p = __q_wait.front();
            __q_wait.pop();
            while (p != marker){
                if (p->b_flag == -1){
                    delete p;
                }else if(p->b_host!=host){
                    __q_wait.push(p);
                }else if(p->b_port!=port){
                    __q_wait.push(p);
                }else{
                    p->b_flag = 0;
                    printf("one host is match\n");
                    break;
                }
                p = __q_wait.front();
                __q_wait.pop();
            }
            if (p == marker){
                printf("\n");
                delete p;
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
    d_peer *marker = NULL;
    unsigned long host = 0;
    unsigned short port = 0;
    while (!__q_peers.empty() && count<5){
        d_peer *p = __q_peers.front();
        if (p == marker){
            break;
        }
        __q_peers.pop();
        if (p->b_flag > 0){
            __q_peers.push(p);
            p->b_flag--;
        }else{
            if (marker == NULL){
                marker = p;
            }
            __q_recall.push(p);
            continue;
        }
        error = b_socket.bsendto(p->b_packet,
                p->b_len, p->b_host, p->b_port);
        assert(error != -1);
        count ++;
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
	d_peer *peer = new d_peer();
    peer->b_flag = 2;
	peer->b_host = inet_addr(host);
	peer->b_port = port;
    peer->b_len = sizeof(__dht_ping);
    peer->b_packet = (char*)__dht_ping;
	__q_peers.push(peer);
    __q_wait.push(peer);
    __play.bwakeup();
    __record.bwakeup();
    return 0;
}
