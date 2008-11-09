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
#include "btimerd.h"
#include "btcodec.h"
#include "bsocket.h"
#include "butils.h"
#include "provider.h"
#include "transfer.h"
#include "kfind.h"
#include "btkad.h"
#include "kbucket.h"

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
    :b_tid(0), b_inited(false)
{
    b_ident = "bdhtnet";
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
    if (b_inited == false){
        b_inited = true;
        bwakeup(NULL);
    }
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
    if (b_inited == false){
        b_inited = true;
        bwakeup(NULL);
    }
    return b_socket.bsendto(__find_node,
            sizeof(__find_node)-1,
            host, port);
}

int
bdhtnet::ping_node(uint32_t tid, uint32_t host, uint16_t port)
{
    memcpy(&__ping_node[47], &tid, 4);
    if (b_inited == false){
        b_inited = true;
        bwakeup(NULL);
    }
    return b_socket.bsendto(__ping_node,
            sizeof(__ping_node)-1,
            host, port);
}

kship *
bdhtnet::get_kship()
{
    int tid = b_tid++;
    kship *transfer = new kship(this, tid);
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
            kship *t = (*b_requests.find(id)).second;
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
bdhtnet::bdocall()
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

bootstraper::bootstraper()
{
    b_transfer = NULL;
}

class checkthread: public bthread
{
    public:
        checkthread();
        virtual int bdocall();

    private:
        int b_state;
        time_t b_random;
        time_t b_start_time;
        time_t b_last_refresh;
};

checkthread::checkthread()
{
    b_state = 0;
    b_random = 60;
    b_start_time = now_time();
    b_last_refresh = now_time();
    b_ident = "checkthread";
}

int
checkthread::bdocall()
{
    int state = b_state;
    while (b_runable){
        b_state = state++;
        switch(b_state)
        {
            case 0:
                btime_wait(b_start_time+180);
                if (now_time() > b_last_refresh+b_random){
                    b_random = (60*15*0.9)+(rand()%(60*15))/5;
                    b_last_refresh = now_time();
                    refresh_routing_table();
                }
                break;
            case 1:
                dump_routing_table();
                b_start_time = now_time();
                state = 0;
                break;
            default:
                assert(0);
                break;
        }
    }
    return 0;
}

class boothread: public bthread
{
    public:
        boothread();
        virtual int bdocall();

    private:
        int b_state;
        time_t b_random;
        time_t b_start_time;
        kfind  *b_find;
};

boothread::boothread()
{
    b_find = NULL;
    b_state = 0;
    b_start_time = now_time();
    b_ident = "boothread";
}

int
boothread::bdocall()
{
    char bootid[20];
    int state = b_state;
    while (b_runable){
        b_state = state++;
        switch(b_state)
        {
            case 0:
                getkadid(bootid);
                bootid[19]^=0x1;
                b_find = btkad::find_node(bootid);
                break;
            case 1:
                b_random = (60*15*0.9)+(rand()%(60*15))/5;
                b_start_time = now_time();
                break;
            case 2:
                if (b_find->vcall() == -1){
                    state = 2;
                }
                break;
            case 3:
                refresh_routing_table();
                btime_wait(b_start_time+b_random);
                break;
            case 4:
                printf("DHT: Refresh Boot!\n");
                delete b_find;
                state = 0;
                break;
            default:
                b_runable = false;
                break;
        }
    }
}

static checkthread __static_checkthread;
static boothread __static_boothread;

int
bdhtnet_start()
{
    static int __call_once_only = 0;

    if (__call_once_only++ > 0){
        return -1;
    }
    char bootid[20];
    genkadid(bootid);
    setkadid(bootid);
    getkadid(&__ping_node[12]);
    getkadid(&__find_node[12]);
    getkadid(&__get_peers[12]);
    __static_boothread.bwakeup(NULL);
    __static_checkthread.bwakeup(NULL);
    return 0;
}
