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
    size_t elen = 0;
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
bdhtnet::query_expand(bdhtcodec *codec, const void *ibuf, size_t len,
        uint32_t host, uint16_t port)
{
    size_t tlen = 0;
    const char *kadid = codec->codec().bget().bget("a").bget("id").c_str(&tlen);
    if (kadid==NULL || tlen!=20){
        printf("bad query packet\n");
        return;
    }
    const char *query = codec->codec().bget().bget("q").c_str(&tlen);
    if (query == NULL || tlen < 4){
        return;
    }
    if (memcmp(query, "get_peers", tlen) == 0){
        printf("get peers\n");
    }else if (memcmp(query, "find_node", tlen) == 0){
        printf("find node\n");
    }else if (memcmp(query, "ping", tlen) == 0){
        char buff[] = "d1:rd2:id20:mnopqrstuvwxyz123456e1:t1:01:y1:re";
        getkadid(&buff[12]);
        const char *t = codec->codec().bget().bget("t").b_str(&tlen);
        memcpy(&buff[36], t, tlen);
        memcpy(&buff[36+tlen], "1:y1:re", 7);
        b_socket.bsendto(buff, 36+7+tlen, host, port);
        printf("ping sended\n");
    }
    return;
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
        query_expand(codec, ibuf, len, host, port);
    }else if (codec->type() == 'e'){
        /* process error */
        printf("error handler call:\n");
    }else {
        /* default type handler */
        printf("unkown bencode packet:\n");
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
