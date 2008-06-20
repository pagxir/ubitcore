#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>
#include <assert.h>
#include <queue>

#include "bthread.h"
#include "bsocket.h"
#include "bdhtnet.h"
#include "btcodec.h"

struct d_peer{
	unsigned short b_flag;
	unsigned short b_port;
	unsigned long b_host;
};

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

static int dump_sha1_id(const char buf[], size_t len)
{
    int i;
    printf("peer id: ");
    for (i=0; i<len; i++)
        printf("%02x", buf[i]&0xff);
    printf("\n");
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
            printf("R: %c %c %s:%d\n", p1?*p1:'@', t2?*t2:'!', inet_ntoa(*(in_addr*)&host), port);
            const char *id = codec.bget().bget("r").bget("id").c_str(&elen);
            dump_sha1_id(id, elen);
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
        error = b_socket.bsendto((char*)__dht_ping,
                sizeof(__dht_ping), p->b_host, p->b_port);
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
	__q_peers.push(peer);
    __q_wait.push(peer);
    __play.bwakeup();
    __record.bwakeup();
    return 0;
}

unsigned char __find_nodes[] = {
  0x64, 0x31, 0x3a, 0x61, 0x64, 0x32, 0x3a, 0x69, 0x64, 0x32, 0x30, 0x3a,
  0xd3, 0x86, 0x4b, 0x67, 0x3f, 0xc9, 0x47, 0xe1, 0xa8, 0xd8, 0x55, 0xd7,
  0x00, 0x32, 0xbc, 0x44, 0x48, 0x56, 0x23, 0xaa, 0x36, 0x3a, 0x74, 0x61,
  0x72, 0x67, 0x65, 0x74, 0x32, 0x30, 0x3a, 0x7f, 0xff, 0xff, 0xff, 0x7f,
  0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xff, 0x7f,
  0xff, 0xff, 0xff, 0x65, 0x31, 0x3a, 0x71, 0x39, 0x3a, 0x66, 0x69, 0x6e,
  0x64, 0x5f, 0x6e, 0x6f, 0x64, 0x65, 0x31, 0x3a, 0x74, 0x38, 0x3a, 0x79,
  0x3b, 0x1b, 0x00, 0x01, 0x00, 0x00, 0x00, 0x31, 0x3a, 0x79, 0x31, 0x3a,
  0x71, 0x65
};
