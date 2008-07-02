/* $Id$ */
#include <time.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

#include "bthread.h"
#include "bqueue.h"
#include "butils.h"


/*
 * \x13BitTorrent protocol\0\0\0\0\0\0\0\0<sha1 info hash><20byte peerid>
 */
static int __failed_count=0;
static int __success_count=0;

const unsigned char __protocol[68] ={
    "\023BitTorrent protocol\0\0\0\0\0\0\0\0"
};

bqueue::bqueue()
    :b_state(0), b_ep(NULL)
{
    b_ident="bqueue";
}

int
bqueue::bfailed()
{
    printf("failed: %s:%d\n", 
            inet_ntoa(*(in_addr*)&b_ep->b_host),
            b_ep->b_port);
    __failed_count++;
    b_state = 0;
    bwakeup();
    return 0;
}

int
ok_hndshake(unsigned char info[68])
{
    int i;
    __success_count++;
    unsigned char *ident = info+28;
    if (memcmp(info, __protocol, 20)!=0){
        printf("BAD protocol: %s!\n", info);
        return -1;
    }
    if (memcmp(info+28, get_info_hash(), 20)!=0){
        return -1;
    }
    printf("[%4d/%-4d]: ",
            __success_count, __failed_count);
    for (i=0; i<20; i++){
        printf("%02x", ident[i]);
    }
    printf(" ");
    for (i=0; i<8; i++){
        printf("%02x", info[20+i]);
    }
    printf("\n");
    return 0;
}

int
bqueue::bdocall(time_t timeout)
{
    int error = 0;
    int state = b_state;
    char buffer[8192];
    while(error != -1){
        b_state = state++;
        switch(b_state)
        {
            case 0:
                if (b_ep != NULL){
                    delete b_ep;
                    b_ep = NULL;
                }
                error = bdequeue(&b_ep);
                break;
            case 1:
                break;
            case 2:
                error = b_ep->b_socket.bconnect(
                        b_ep->b_host, b_ep->b_port);
                break;
            case 3:
                memcpy(buffer, __protocol, sizeof(__protocol));
                memcpy(buffer+28, get_info_hash(), 20);
                memcpy(buffer+48, get_peer_ident(), 20);
                error = b_ep->b_socket.bsend(buffer, 68);
                break;
            case 4:
                if (error != 68){
                    printf("%d handshake byte is send!\n", error);
                }
                assert(error == 68);
                break;
            case 5:
                error = b_ep->b_socket.breceive(buffer, 68);
                break;
            case 6:
                if (error != 68){
                    __failed_count++;
                    printf("%d handshake byte is read!\n", error);
                }else if (ok_hndshake((unsigned char*)buffer)==0){
                    bready_push(b_ep);
                    b_ep = NULL;
                }
                break;
            default:
                state = 0;
                break;
        }
    }
    return error;
}
