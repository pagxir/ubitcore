#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <queue>
#include <stack>
#include <map>
#include <stdint.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "bthread.h"
#include "bsocket.h"
#include "btcodec.h"
#include "butils.h"
#include "provider.h"
#include "transfer.h"

kship::kship(bdhtnet *net, uint32_t ident)
{
    b_ident = ident;
    b_dhtnet = net;
}

kship::~kship()
{
    while (!b_queue.empty()){
        bdhtpack *pkg = b_queue.front();
        b_queue.pop();
        free(pkg->b_ibuf);
        delete pkg;
    }
    b_dhtnet->detach(b_ident);
}

int
kship::get_response(void *buf, size_t len,
        uint32_t *host, uint16_t *port)
{
    if (b_queue.empty()){
        b_thread = bthread::now_job();
        return -1;
    }
    bdhtpack *pkg = b_queue.front();
    assert(pkg!=NULL);
    if (pkg->b_len > len){
        assert(pkg->b_len < len);
        return -1;
    }
    b_queue.pop();
    memcpy(buf, pkg->b_ibuf, pkg->b_len);
    if (host != NULL){
        *host = pkg->b_host;
    }
    if (port != NULL){
        *port = pkg->b_port;
    }
    free(pkg->b_ibuf);
    int count = pkg->b_len;
    delete pkg;
    return count;
}

int
kship::get_peers(uint32_t host, 
        uint16_t port, uint8_t ident[20])
{
    return b_dhtnet->get_peers(b_ident, host, port, ident);
}

int
kship::find_node(uint32_t host, 
        uint16_t port, uint8_t ident[20])
{
    return b_dhtnet->find_node(b_ident, host, port, ident);
}

int
kship::ping_node(uint32_t host, uint16_t port)
{
    return b_dhtnet->ping_node(b_ident, host, port);
}

void
kship::binput(bdhtcodec *codec, const void *ibuf, size_t len,
        uint32_t host, uint16_t port)
{
    bdhtpack *pkg = new bdhtpack;
    pkg->b_ibuf = malloc(len);
    pkg->b_len = len;
    pkg->b_host = host;
    pkg->b_port = port;
    assert(pkg->b_ibuf != NULL);
    memcpy(pkg->b_ibuf, ibuf, len);
    if (b_thread != NULL){
        b_thread->bwakeup(b_thread);
        b_thread = NULL;
    }
    b_queue.push(pkg);
}

