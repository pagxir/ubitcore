/* $Id: bdhtnet.cc 94 2008-07-19 04:12:35Z pagxir $ */
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <queue>
#include <stack>
#include <map>
#include <stdint.h>
#include <stdlib.h>

#include "buinet.h"
#include "bthread.h"
#include "bsocket.h"
#include "btcodec.h"
#include "butils.h"
#include "bdhtident.h"
#include "bdhtnet.h"

static bdhtnet __dhtnet;

uint8_t __ping_nodes[] = {
    "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t4:PPPP1:y1:qe"
};

uint8_t __find_nodes[] = {
    "d1:ad2:id20:abcdefghij01234567896:target"
        "20:abcdefghij0123456789e1:q9:find_node1:t4:FFFF1:y1:qe"
};

struct bdhtpack
{
    void *b_ibuf;
    size_t b_len;
    uint32_t b_host;
    uint16_t b_port;
    uint16_t b_align;
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

bdhtransfer::bdhtransfer(bdhtnet *net, uint32_t ident)
{
    b_ident = ident;
    b_dhtnet = net;
    b_poller = NULL;
}

int
bdhtransfer::get_response(void *buf, size_t len,
        uint32_t *host, uint16_t *port)
{
    if (b_queue.empty()){
        return -1;
    }
    bdhtpack *pkg = b_queue.front();
    if (pkg->b_len > len){
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
    delete pkg;
    return pkg->b_len;
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

bdhtnet::bdhtnet()
    :b_tid(0)
{
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

    memcpy(__find_nodes+43, ident, 20);
    memcpy(__find_nodes+83, &tid, 4);
    return b_socket.bsendto(__find_nodes,
            sizeof(__find_nodes)-1,
            host, port);
}

int
bdhtnet::ping_node(uint32_t tid, uint32_t host, uint16_t port)
{
    memcpy(__ping_nodes+47, &tid, 4);
    return b_socket.bsendto(__ping_nodes,
            sizeof(__ping_nodes)-1,
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
        }else{
            printf("not found: %d!\n", id);
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
            binput(&codec, buffer, error, host, port);
        }else{
            printf("recv bad packet !\n");
        }
#if 0
        in_addr addr;
        memcpy(&addr, &host, 4);
        printf("recv one packet: %s:%d\n", inet_ntoa(addr), htons(port));
#endif
        error = b_socket.brecvfrom(buffer, sizeof(buffer), &host, &port);
    }
    return error;
}

void
bdhtbucket::add_node(uint32_t host, uint16_t port)
{
    uint8_t ibuf[20]={0};
    netpt pt(host,port);
    ibuf[19] = b_index++;
    bdhtident dident(ibuf);
    bootstraper *traper = new bootstraper();
    traper->b_host = host;
    traper->b_port = port;
    traper->b_transfer = __dhtnet.get_transfer();
    b_trapmap.insert(std::make_pair(pt, traper));
    b_bootmap.insert(std::make_pair(dident, traper));
}

static uint8_t _bootraphash[20];

bdhtbucket::bdhtbucket()
{
    b_index = 0;
    b_state = 0;
}

typedef struct _npack
{
    uint8_t ident[20];
    uint8_t host[4];
    uint8_t port[2];
}npack;

void
bdhtbucket::find_next(const void *buf, size_t len)
{
    size_t elen;
    btcodec codec;
    codec.bload((char*)buf, len);

    const char *np = codec.bget().bget("r").bget("nodes").c_str(&elen);
    if (np == NULL){
        printf("bad packet\n");
        return ;
    }

    npack *p, *ep=(npack*)(np+elen);
    for (p=(npack*)(np); p<ep; p++){
        bdhtident dident(p->ident);
        bootstraper *traper = new bootstraper();
        memcpy(&traper->b_host, &p->host, 4);
        memcpy(&traper->b_port, &p->port, 2);
        traper->b_port = htons(traper->b_port);
        netpt pt(traper->b_host, traper->b_port);
        if (false == b_trapmap.insert(std::make_pair(pt, traper)).second){
            delete traper;
            continue;
        }
#if 0
        int i;
        for (i=0; i<20; i++){
            printf("%02x", p->ident[i]);
        }
        printf("\t");
        for (i=0; i<20; i++){
            printf("%02x", get_peer_ident()[i]);
        }
        printf("\n");
#endif
        if (b_bootmap.insert(std::make_pair(dident, traper)).second == false){
            delete traper;
            printf("node reenter DHT network!\n");
            continue;
        }
        traper->b_transfer = __dhtnet.get_transfer();
    }
}

typedef uint8_t pident[20];
static uint8_t *__bucket[160][8];

void
update_route(const void *ibuf, size_t len, uint32_t host, uint16_t port)
{
    size_t idl;
    btcodec codec;
    codec.bload((char*)ibuf, len);

    const char *ident = codec.bget().bget("r").bget("id").c_str(&idl);
    if (ident==NULL || idl!=20){
        return;
    }

    int i;
#if 0
    printf("update route: ");
    for (i=0; i<20; i++){
        printf("%02x", ident[i]&0xff);
    }
    printf("\t");
    for (i=0; i<20; i++){
        printf("%02x", get_peer_ident()[i]);
    }
    printf("\n");
#endif
    fprintf(stderr, ".");
    bdhtident one((uint8_t*)ident), self((uint8_t*)get_peer_ident());
    bdhtident dist = one^self;
    int lg = dist.lg();
    for (i=0; i<8; i++){
        if (__bucket[lg][i] == NULL){
            __bucket[lg][i] = new pident;
            memcpy(__bucket[lg][i], ident, 20);
            break;
        }
    }
}

static void
dump_route_table()
{
    int i, j;
    printf("\nroute table begin\n");
    printf("myid: ");
    for (i=0; i<20; i++){
        printf("%02x", 0xff&(get_peer_ident()[i]));
    }
    printf("---------------\n");
    for (i=0; i<160; i++){
        for (j=0; j<8; j++){
            uint8_t *vp = __bucket[i][j];
            if (vp != NULL){
                int u;
                printf("0x%02x:\t", i);
                for (u=0; u<20; u++){
                    printf("%02x", vp[u]);
                }
                printf("\n");
            }
        }
    }
    printf("route table finish\n");
}

int
bdhtbucket::bdocall(time_t timeout)
{
    int i;
    uint32_t host;
    uint16_t port;
    int error = 0;
    int state = b_state;
    char buffer[8192];
    std::map<bdhtident, bootstraper*>::iterator iter, niter;

    while (error != -1){
        b_state = state++;
        switch (b_state){
            case 0:
                for (iter=b_bootmap.begin();
                        iter!=b_bootmap.end(); ){
                    niter = iter++;
                    bootstraper &p = *niter->second;
                    bdhtransfer *trans = p.b_transfer;
                    if (trans == NULL){
                        b_bootmap.erase(niter);
                        continue;
                    }
                    trans->find_node(p.b_host,
                            p.b_port, _bootraphash);
                    b_tryagain.push(niter->second);
                    b_findmap.insert(*niter);
                    b_bootmap.erase(niter);
                }
                break;
            case 1:
                b_polling = true;
                b_touch = time(NULL);
                break;
            case 2:
                error = btime_wait(b_touch+2);
                for (iter=b_findmap.begin();
                        iter!=b_findmap.end(); ){
                    niter = iter++;
                    bootstraper &p = *niter->second;
                    bdhtransfer *trans = p.b_transfer;
                    if (trans == NULL){
                        b_findmap.erase(niter);
                        continue;
                    }
                    int flag = trans->get_response(
                            buffer, sizeof(buffer),
                            &host, &port);
                    if (flag == -1){
                        continue;
                    }
                    delete trans;
                    p.b_transfer = NULL;
                    state = error = 0;
                    find_next(buffer, flag);
                    update_route(buffer, flag, host, port);
                }
                break;
            case 3:
                while (!b_tryfinal.empty()){
                    bootstraper &p = *b_tryfinal.front();
                    p.b_transfer = NULL;
                    b_tryfinal.pop();
                }
                while (!b_tryagain.empty()){
                    bootstraper &p = *b_tryagain.front();
                    bdhtransfer *trans = p.b_transfer;
                    b_tryagain.pop();
                    if (trans == NULL){
                        continue;
                    }
                    trans->find_node(p.b_host,
                            p.b_port, _bootraphash);
                    b_tryfinal.push(&p);
                }
                break;
            case 4:
                if (!b_findmap.empty()){
                    state = 1;
                }
                break;
            default:
                dump_route_table();
                return 0;
                break;
        }
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
        memcpy(_bootraphash, get_peer_ident(), 20);
        _bootraphash[19]^=0x1;
    }
    __bootstrap_bucket.bwakeup();
    __dhtnet.bwakeup();
    return 0;
}
