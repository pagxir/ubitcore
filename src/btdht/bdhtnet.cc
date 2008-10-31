/* $Id: bdhtnet.cc 94 2008-07-19 04:12:35Z pagxir $ */
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <queue>
#include <map>

#include "arpa/inet.h"
#include "btcodec.h"
#include "bsocket.h"
#include "butils.h"
#include "bdhtident.h"
#include "bdhtnet.h"
#include "bdhtboot.h"
#include "bdhtransfer.h"

bdhtnet __dhtnet;
static bdhtboot __boot_bucket(&__dhtnet, 159);

char __ping_node[] = {
    "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t4:PPPP1:y1:qe"
};

char __find_node[] = {
    "d1:ad2:id20:abcdefghij01234567896:target"
        "20:abcdefghij0123456789e1:q9:find_node1:t4:FFFF1:y1:qe"
};

char __get_peers[] =  {
    "d1:ad2:id20:abcdefghij01234567899:info_hash"
        "20:mnopqrstuvwxyz123456e1:q9:get_peers1:t4:GGGG1:y1:qe"
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
    if (t!=NULL && elen<=4){
        b_transid = 0;
        memcpy(&b_transid, transid, elen);
    }
    return (error=0);
}

bdhtnet::bdhtnet()
    :b_tid(0)
{
}

int
bdhtnet::detach(uint32_t id)
{
    b_requests.erase(id);
    return 0;
}

int
bdhtnet::get_peers(uint32_t tid, uint32_t host,
        uint16_t port, uint8_t ident[20])
{
    memcpy(&__get_peers[46], ident, 20);
    memcpy(&__get_peers[86], &tid, 4);
    return b_socket.bsendto(__get_peers,
            sizeof(__get_peers)-1,
            host, port);
}

int
bdhtnet::find_node(uint32_t tid, uint32_t host,
        uint16_t port, uint8_t ident[20])
{

#if 0
    in_addr addr;
    memcpy(&addr, &host, 4);
    printf("find node: %s:%d\n",
            inet_ntoa(addr), htons(port));
#endif

    memcpy(&__find_node[43], ident, 20);
    memcpy(&__find_node[83], &tid, 4);
    return b_socket.bsendto(__find_node,
            sizeof(__find_node)-1,
            host, port);
}

int
bdhtnet::ping_node(uint32_t tid, uint32_t host, uint16_t port)
{
    memcpy(&__ping_node[47], &tid, 4);
    return b_socket.bsendto(__ping_node,
            sizeof(__ping_node)-1,
            host, port);
}

bdhtransfer *
bdhtnet::get_transfer()
{
    int tid = b_tid++;
    bdhtransfer *transfer = new bdhtransfer(this, tid);
    b_requests.insert(std::make_pair(tid, transfer));
    return transfer;
}

void
bdhtnet::binput(bdhtcodec *codec, const void *ibuf, size_t len,
        uint32_t host, uint16_t port)
{
    if (codec->type() == 'r'){
        /* process response */
        int id = codec->transid();
        if (b_requests.find(id) != b_requests.end()){
            bdhtransfer *t = (*b_requests.find(id)).second;
            t->binput(codec, ibuf, len,host, port);
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
    in_addr_t host;
    unsigned short port;
    char buffer[8192];
    int error = b_socket.brecvfrom(buffer, sizeof(buffer), &host, &port);
    while (error != -1){ 
        bdhtcodec codec;
        if (0==codec.bload(buffer, error)){
            binput(&codec, buffer, error, host, port);
        }
        error = b_socket.brecvfrom(buffer, sizeof(buffer), &host, &port);
    }
    return error;
}


int
bdhtnet_node(const char *host, int port)
{
    uint32_t ihost = inet_addr(host);
    __boot_bucket.add_dhtnode(ihost, port);
    return 0;
}

bool
bdhtpoller::polling()
{
    return b_polling;
}

bootstraper::bootstraper()
{
    b_transfer = NULL;
}

class boothread: public bthread
{
    public:
        boothread();
        virtual int bdocall(time_t timeout);

    private:
        int b_state;
        time_t b_start_time;
};

boothread::boothread()
{
    b_state = 0;
    b_start_time = now_time();
}

int
boothread::bdocall(time_t timeout)
{
    char bootid[20];
    int state = b_state;
    while (b_runable){
        b_state = state++;
        switch(b_state)
        {
            case 0:
                getclientid(bootid);
                bootid[19]^=0x1;
                b_start_time = now_time();
                __boot_bucket.set_target((uint8_t*)bootid);
                __boot_bucket.bwakeup();
                __dhtnet.bwakeup();
                break;
            case 1:
                btime_wait(b_start_time+30);
                break;
            case 2:
                printf("DHT: Hello World!\n");
                break;
            default:
                b_runable = false;
                break;
        }
    }
}

static boothread __static_boothread;

int
bdhtnet_start()
{
    static int __call_once_only = 0;

    if (__call_once_only++ > 0){
        return -1;
    }
    getclientid(&__ping_node[12]);
    getclientid(&__find_node[12]);
    getclientid(&__get_peers[12]);
    __static_boothread.bwakeup();
    return 0;
}
