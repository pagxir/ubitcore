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
#include "ident.h"
#include "provider.h"
#include "transfer.h"

    extern bdhtnet __dhtnet;
bdhtransfer::bdhtransfer(bdhtnet *net, uint32_t ident)
{
    b_ident = ident;
    b_dhtnet = net;
    b_poller = NULL;
}

bdhtransfer::~bdhtransfer()
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
bdhtransfer::get_response(bdhtpoller *poller, void *buf, size_t len,
        uint32_t *host, uint16_t *port)
{
    assert(b_dhtnet==&__dhtnet);
    if (b_queue.empty()){
        bdopolling(poller);
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
    delete pkg;
    return pkg->b_len;
}

int
bdhtransfer::get_peers(uint32_t host, 
        uint16_t port, uint8_t ident[20])
{
    return b_dhtnet->get_peers(b_ident, host, port, ident);
}

int
bdhtransfer::find_node(uint32_t host, 
        uint16_t port, uint8_t ident[20])
{
    return b_dhtnet->find_node(b_ident, host, port, ident);
}

int
bdhtransfer::ping_node(uint32_t host, uint16_t port)
{
    return b_dhtnet->ping_node(b_ident, host, port);
}

void
bdhtransfer::binput(bdhtcodec *codec, const void *ibuf, size_t len,
        uint32_t host, uint16_t port)
{
    assert(b_dhtnet==&__dhtnet);
    bdhtpack *pkg = new bdhtpack;
    pkg->b_ibuf = malloc(len);
    pkg->b_len = len;
    pkg->b_host = host;
    pkg->b_port = port;
    assert(pkg->b_ibuf != NULL);
    memcpy(pkg->b_ibuf, ibuf, len);
    b_queue.push(pkg);
    if (b_poller == NULL){
        return;
    }
    if (b_poller->polling()){
        b_poller->polling_dump();
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
#if 1
    if (poller != b_poller){
        if (b_poller != NULL){
            printf("warn: bdhtransfer::bpoll conflict!\n");
        }
    }
#endif
    b_poller = poller;
    return -1;
}
