/* $Id$ */
#include <time.h>
#include <string.h>
#include <assert.h>

#include "bthread.h"
#include "bqueue.h"
#include "butils.h"


/*
 * \x13BitTorrent protocol\0\0\0\0\0\0\0\0<sha1 info hash><20byte peerid>
 */
const unsigned char __protocol[68] ={
    "\023BitTorrent protocol\0\0\0\0\0\0\0\0"
};

bqueue::bqueue()
    :b_state(0)
{
    b_ident="bqueue";
}

int
bqueue::bfailed()
{
    b_state = 0;
    bwakeup();
    return 0;
}

int
dd_ident(unsigned char ident[20])
{
    int i;
    printf("remote peed ident: ");
    for (i=0; i<20; i++){
        printf("%02x", ident[i]);
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
                error = bdequeue(&b_ep);
                break;
            case 1:
                b_socket.reset(new bsocket());
                break;
            case 2:
                error = b_socket->bconnect(b_ep.b_host, b_ep.b_port);
                break;
            case 3:
                memcpy(buffer, __protocol, sizeof(__protocol));
                memcpy(buffer+28, get_info_hash(), 20);
                memcpy(buffer+48, get_peer_ident(), 20);
                error = b_socket->bsend(buffer, 68);
                break;
            case 4:
#ifndef NDEBUG
                if (error == 0){
                    state = 3;
                    break;
                }
                if (error != 68){
                    printf("%d handshake byte is send!\n", error);
                }
#endif
                assert(error==68);
                error = b_socket->breceive(buffer, 68);
                break;
            case 5:
                if (error != 68){
                    printf("%d handshake byte is read!\n", error);
                }else{
                    dd_ident((unsigned char*)buffer+48);
                }
                break;
            default:
                state = 0;
                break;
        }
    }
    return error;
}
