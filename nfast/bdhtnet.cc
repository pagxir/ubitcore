#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>
#include "bthread.h"
#include "bsocket.h"
#include "bdhtnet.h"

class bdhtnet: public bthread
{
    public:
        bdhtnet(unsigned long host, int port);
        ~bdhtnet();
        virtual int bdocall(time_t timeout);
    private:
        bsocket b_socket;
        int b_state;
        int b_port;
        unsigned long b_host;
        time_t b_last;
};

bdhtnet::bdhtnet(unsigned long host, int port):
    b_port(port), b_host(host)
{
    b_state = 0;
}

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
    int error = 0;
    int state = b_state;
    unsigned long host = 0;
    unsigned short port = 0;
    char buffer[8192];
    while(error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                b_last = time(NULL);
                error = b_socket.bsendto((char*)__dht_ping, sizeof(__dht_ping),
                        b_host, b_port);
                break;
            case 1:
                error = b_socket.brecvfrom(buffer, sizeof(buffer), &host, &port);
                if (error==-1 && btime_wait(b_last+10)!=-1){
                    printf("ping time out!\n");
                    state = error = 0;
                }
                break;
            case 2:
                if (error >= 0){
                    printf("Hello World: %s:%d!\n", inet_ntoa(*(in_addr*)&host), port);
                }
                break;
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

int
bdhtnet_node(const char *host, int port)
{
    bdhtnet *bdht = new bdhtnet(inet_addr(host), port);
    bdht->bwakeup();
    return 0;
}
