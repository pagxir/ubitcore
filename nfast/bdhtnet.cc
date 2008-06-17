#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>
#include <queue>

#include "bthread.h"
#include "bsocket.h"
#include "bdhtnet.h"

struct d_peer{
	unsigned short b_flag;
	unsigned short b_port;
	unsigned long b_host;
};

std::queue<d_peer> __q_peers;
std::queue<d_peer> __q_wait;

class bdhtnet: public bthread
{
    public:
        bdhtnet();
        ~bdhtnet();
        virtual int bdocall(time_t timeout);
    private:
        bsocket b_socket;
        int b_state;
		int b_count;
        time_t b_last;
};

bdhtnet::bdhtnet():
    b_count(0), b_state(0)
{
}

extern unsigned char __find_nodes[98];

unsigned char __dht_ping[] = {
  0x64, 0x31, 0x3a, 0x61, 0x64, 0x32, 0x3a, 0x69, 0x64, 0x32, 0x30, 0x3a,
  0xd3, 0x86, 0x4b, 0x67, 0x3f, 0xc9, 0x47, 0xe1, 0xa8, 0xd8, 0x55, 0xd7,
  0x00, 0x32, 0xbc, 0x44, 0x48, 0x56, 0x23, 0xaa, 0x65, 0x31, 0x3a, 0x71,
  0x34, 0x3a, 0x70, 0x69, 0x6e, 0x67, 0x31, 0x3a, 0x74, 0x38, 0x3a, 0x51,
  0x3b, 0x1b, 0x00, 0x01, 0x00, 0x00, 0x00, 0x31, 0x3a, 0x79, 0x31, 0x3a,
  0x71, 0x65
};

int
bdhtnet::bdocall(time_t timeout)
{
	d_peer peer;
    int error = 0;
    int state = b_state;
    unsigned long host = 0;
    unsigned short port = 0;
    char buffer[8192];
    while(error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
				if (b_count == 0){
				   	if (!__q_peers.empty()){
					   	peer = __q_peers.front();
					   	b_last = time(NULL);
					   	error = b_socket.bsendto((char*)__dht_ping, sizeof(__dht_ping),
							   	peer.b_host, peer.b_port);
					   	__q_wait.push(peer);
					   	__q_peers.pop();
					   	b_count++;
#if 0
					}else if (!can_find_more_node()){
					   	error = find_more_node();
				   	}else if (!can_find_more_peer()){
					   	error = find_more_peer();
#endif
					}
			   	}
                break;
            case 1:
                error = b_socket.brecvfrom(buffer, sizeof(buffer), &host, &port);
                if (error==-1 && btime_wait(b_last+1)!=-1){
                    if (b_count > 0){
                        state = error = 0;
                        if (!__q_wait.empty()){
                            if (__q_wait.front().b_flag > 0){
                                __q_wait.front().b_flag --;
                                __q_peers.push(__q_wait.front());
                            }
                            __q_wait.pop();
                        }
                        b_count--;
                    }else{
                        /* Time to update bluck! */
                        printf("Time to update bluck\n");
                        state = error = 0;
                        b_last = time(NULL)+15;
                    }
                }
                break;
            case 2:
                printf("Hello World: %s:%d!\n", inet_ntoa(*(in_addr*)&host), port);
                if (error>=0 && b_count>0){
					__q_wait.pop();
					b_count--;
					state = 0;
                }else{
                    b_last = time(NULL)+15;
                    state = 1;
                }
                break;
#if 0
			case 3:
				error = btime_wait(b_last+15*60);
				break;
			case 4:
				update_bluck();
				break;
#endif
            default:
                error = -1;
                break;
        }
    }
    return error;
}

bdhtnet::~bdhtnet()
{
}

static bdhtnet __dhtnet;

int
bdhtnet_node(const char *host, int port)
{
	d_peer peer;
    peer.b_flag = 3;
	peer.b_host = inet_addr(host);
	peer.b_port = port;
	__q_peers.push(peer);
	__dhtnet.bwakeup();
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
