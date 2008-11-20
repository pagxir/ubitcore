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

int padto(char buff[], kitem_t items[], size_t n)
{
    int i = 0;
    int count = 0;
    count = sprintf(buff, "%d:", n*26);
    for (i=0; i<n; i++){
        memcpy(&buff[count], items[i].kadid, 20);
        count += 20;
        memcpy(&buff[count], &items[i].host, 4);
        count += 4;
        memcpy(&buff[count], &items[i].port, 2);
        count += 2;
    }
    return count;
}

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
    btcodec& btc = codec->codec();
    const char *kadid = btc.bget().bget("a").bget("id").c_str(&tlen);
    if (kadid==NULL || tlen!=20){
        printf("bad query packet\n");
        return;
    }
    const char *query = btc.bget().bget("q").c_str(&tlen);
    if (query == NULL || tlen < 4){
        return;
    }
    if (memcmp(query, "get_peers", tlen) == 0){
        size_t target_len = 0;
        const char *target = btc.bget().bget("a").bget("info_hash").c_str(&target_len);
        if (target_len==20 && target!=NULL){
            printf("get peers: %s\n", idstr(target));
            kitem_t item, items[8];
            int n = find_nodes(target, items, true);
            char buff[8192] = {
                "d1:rd5:token5:noown2:id20:XXXXXXXXXXXXXXXXXXXX5:nodes"
                    "26:NNNNNNNNNNNNNNNNNNNNNNNNNNe1:t1:01:y1:re" 
            };
            getkadid(&buff[12+14]);
            int n1 = padto(&buff[39+14], items, n);
            const char *t = btc.bget().bget("t").b_str(&tlen);
            memcpy(&buff[n1+39+14], "e1:t", 4);
            memcpy(&buff[n1+43+14], t, tlen);
            memcpy(&buff[n1+43+14+tlen], "1:y1:re", 7);
            b_socket.bsendto(buff, 14+n1+43+7+tlen, host, port);
            memcpy(item.kadid, kadid, 20);
            item.host = host;
            item.port = port;
            update_contact(&item, false);
        }
    }else if (memcmp(query, "find_node", tlen) == 0){
        size_t target_len = 0;
        const char *target = btc.bget().bget("a").bget("target").c_str(&target_len);
        if (target_len==20 && target!=NULL){
            kitem_t item, items[8];
            int n = find_nodes(target, items, true);
            char buff[8192] = {
                "d1:rd2:id20:XXXXXXXXXXXXXXXXXXXX5:nodes"
                    "26:NNNNNNNNNNNNNNNNNNNNNNNNNNe1:t1:01:y1:re" 
            };
            getkadid(&buff[12]);
            int n1 = padto(&buff[39], items, n);
            const char *t = btc.bget().bget("t").b_str(&tlen);
            memcpy(&buff[n1+39], "e1:t", 4);
            memcpy(&buff[n1+43], t, tlen);
            memcpy(&buff[n1+43+tlen], "1:y1:re", 7);
            b_socket.bsendto(buff, n1+43+7+tlen, host, port);
            memcpy(item.kadid, kadid, 20);
            item.host = host;
            item.port = port;
            update_contact(&item, false);
        }else{
            printf("find node unkown now");
        }
    }else if (memcmp(query, "ping", tlen) == 0){
        char buff[] = "d1:rd2:id20:mnopqrstuvwxyz123456e1:t1:01:y1:re";
        getkadid(&buff[12]);
        const char *t = btc.bget().bget("t").b_str(&tlen);
        memcpy(&buff[36], t, tlen);
        memcpy(&buff[36+tlen], "1:y1:re", 7);
        b_socket.bsendto(buff, 36+7+tlen, host, port);
        /* printf("ping sended\n"); */
    }else {
        std::string text(query, tlen);
        printf("unkown query: %s\n", text.c_str());
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
