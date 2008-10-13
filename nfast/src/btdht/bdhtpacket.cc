/* $Id: bdhtnet.cc 94 2008-07-19 04:12:35Z pagxir $ */
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <queue>
#include <stack>
#include <map>
#include <stdint.h>

#include "buinet.h"
#include "bthread.h"
#include "bsocket.h"
#include "btcodec.h"
#include "butils.h"
#include "bdhtnet.h"

uint8_t __ping_nodes[] = {
    "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t1:P1:y1:qe"
};

uint8_t __find_nodes[] = {
    "d1:ad2:id20:abcdefghij01234567896:target"
        "20:abcdefghij0123456789e1:q9:find_node1:t1:F1:y1:qe"
};

int
bdhtcodec::bload(const char *buffer, size_t length)
{
    int error = -1;
    size_t elen = -1;
    b_codec.bload(buffer, length);
    const char *p = b_codec.bget().bget("y").c_str(&elen);
    if (p==NULL || elen!=1){
        return error;
    }
    b_type = *p;
    const char *t = b_codec.bget().bget("t").b_str(&elen);
    if (t==NULL || elen<3){
        return error;
    }
    b_ident = std::string(t, elen);
    const char *transid = b_codec.bget().bget("t").c_str(&elen);
    if (t!=NULL && elen<4){
        b_transid = 0;
        memcpy(&b_transid, transid, elen);
    }
    return error;
}

void
bdhtransfer::binput(bdhtcodec *codec, uint32_t host, uint16_t port)
{
    b_queue.push(host);
    if (b_poller == NULL){
        return;
    }
    if (b_poller->polling()){
        b_poller->bwakeup();
    }
    b_poller = NULL;
}

int
bdhtransfer::bdopolling(bdhtpoller *poller)
{
    if (!b_queue.empty()){
        return 0;
    }
#if 0
    if (poller != b_poller){
        if (b_poller != NULL){
            printf("warn: bdhtransfer::bpoll conflict!\n");
        }
    }
#endif
    b_poller = poller;
    return -1;
}

static bdhtnet __dhtnet;

bdhtnet::bdhtnet()
    :b_tid(0)
{
}

int
bdhtnet::ping_node(uint32_t tid, uint32_t host, uint16_t port)
{
    return b_socket.bsendto(__ping_nodes,
            sizeof(__ping_nodes)-1,
            host, port);
}

bdhtransfer *
bdhtnet::get_transfer()
{
    return new bdhtransfer(this, b_tid++);
}

void
bdhtnet::binput(bdhtcodec *codec, uint32_t host, uint16_t port)
{
    if (codec->type() == 'r'){
        /* process response */
        int id = codec->transid();
        if (b_requests.find(id) != b_requests.end()){
            (*b_requests.find(id)).second->binput(codec, host, port);
        }
    }else if (codec->type() == 'q'){
        /* process query */
    }else if (codec->type() == 'e'){
        /* process error */
    }else {
        /* default type handler */
    }
    return;
}

int
bdhtnet::bdocall(time_t timeout)
{
    unsigned long host;
    unsigned short port;
    char buffer[8192];
    int error = b_socket.brecvfrom(buffer, sizeof(buffer), &host, &port);
    while (error != -1){ 
        bdhtcodec codec;
        if (0==codec.bload(buffer, error)){
            binput(&codec, host, port);
        }else{
            printf("recv bad packet !\n");
        }
        error = b_socket.brecvfrom(buffer, sizeof(buffer), &host, &port);
    }
    return error;
}

void
bdhtbucket::add_node(uint32_t host, uint16_t port)
{
    if (b_count < 8){
        b_index = b_count++;
    }else {
        b_index++;
    }
    if (b_index == 8){
        b_index = 0;
    }
    b_port_list[b_index] = port;
    b_host_list[b_index] = host;
}

int
bdhtbucket::bdocall(time_t timeout)
{
    int i;
    int error = -1;

    if (b_transfer == NULL){
        b_transfer = __dhtnet.get_transfer();
    }
    for (i=0; i<b_count; i++){
#if 0
        b_transfer->ping_node(b_host_list[i],
                b_port_list[i]);
#endif
    }

    return error;
}

static bdhtbucket __bootstrap_bucket;

int
bdhtnet_node(const char *host, int port)
{
    uint32_t ihost = inet_addr(host);
    __bootstrap_bucket.add_node(ihost, port);
    return 0;
}

int
bdhtnet_start()
{
    static int __call_once_only = 0;

    if (__call_once_only++ == 0){
        memcpy(__ping_nodes+12, get_peer_ident(), 20);
        memcpy(__find_nodes+12, get_peer_ident(), 20);
        memcpy(__find_nodes+43, get_peer_ident(), 20);
        __find_nodes[62]^=0x1;
    }
    __bootstrap_bucket.bwakeup();
    __dhtnet.bwakeup();
    return 0;
}
