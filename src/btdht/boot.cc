#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <queue>
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
#include "boot.h"
#include "transfer.h"
#include "route.h"

typedef struct _npack
{
    uint8_t ident[20];
    uint8_t host[4];
    uint8_t port[2];
}npack;


bdhtboot::bdhtboot(bdhtnet *dhtnet, int tableid)
{
    b_count = 0;
    b_state = 0;
    b_dhtnet = dhtnet;
    b_tableid = tableid;
}

void
bdhtboot::set_target(uint8_t target[20])
{
    memcpy(b_target, target, 20);
}

void
bdhtboot::add_dhtnode(uint32_t host, uint16_t port)
{
    int i;
    int end=std::min(8, b_count);
    for (i=0; i<end; i++){
        if (host == b_hosts[i]){
            return ;
        }
    }
    int idx = 0x7&b_count++;
    b_hosts[idx] = host;
    b_ports[idx] = port;
}

void
bdhtboot::polling_dump()
{

}


void
bdhtboot::find_node_next(const void *buf, size_t len)
{
    size_t elen;
    btcodec codec;
    codec.bload((char*)buf, len);
    char clientid[20];
    getclientid(clientid);
    bdhtident self((uint8_t*)clientid);

    const char *ident = codec.bget().bget("r").bget("id").c_str(&elen);
    if (ident!=NULL && elen==20){
        bdhtident node((uint8_t*)ident); 
        bdhtident dist = node^self;
        b_filter.insert(std::make_pair(dist, node));
        if (b_filter.size() > 8){
            b_filter.erase(--b_filter.end());
        }
    }
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
        if (b_filter.size()>=8){
            bdhtident dist = dident^self;
            if ((--b_filter.end())->first < dist){
                delete traper;
                continue;
            }
        }
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
            continue;
        }
        traper->b_transfer = b_dhtnet->get_transfer();
    }
}

void
bdhtboot::add_node_list(uint32_t hosts[], uint16_t ports[],
        uint8_t ident[][20], size_t count)
{
    int i;
    uint8_t ibuf[20]={0};
    for (i=0; i<count; i++){
        bdhtident dident(ident[i]);
        bootstraper *traper = new bootstraper();
        netpt pt(hosts[i], ports[i]);
        if (false == b_trapmap.insert(std::make_pair(pt, traper)).second){
            delete traper;
            continue;
        }
        if (b_bootmap.insert(std::make_pair(dident, traper)).second == false){
            continue;
        }
        traper->b_transfer = b_dhtnet->get_transfer();
    }
}

extern bdhtnet __dhtnet;

int
bdhtboot::bdocall(time_t timeout)
{
    int i;
    uint32_t host;
    uint16_t port;
    int error = 0;
    int state = b_state;
    char buffer[8192];
    std::map<netpt, bootstraper*>::iterator inter;
    std::map<bdhtident, bootstraper*>::iterator iter, niter;

    assert(b_dhtnet == &__dhtnet);

    while (error != -1){
        b_state = state++;
        switch (b_state){
            case 0:
                for (i=0; i<8; i++){
                    if (i >= b_count){
                        break;
                    }
                    host = b_hosts[i];
                    port = b_ports[i];
                    uint8_t ibuf[20]={0};
                    netpt pt(host,port);
                    ibuf[19] = i;
                    bdhtident dident(ibuf);
                    bootstraper *traper = new bootstraper();
                    traper->b_host = host;
                    traper->b_port = port;
                    traper->b_transfer = b_dhtnet->get_transfer();
                    b_trapmap.insert(std::make_pair(pt, traper));
                    b_bootmap.insert(std::make_pair(dident, traper));
                }
                break;
            case 1:
                if (route_need_update(b_tableid)){
                    fprintf(stderr, "F%d] ", b_tableid);
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
                                p.b_port, b_target);
                        b_findmap.insert(*niter);
                        b_bootmap.erase(niter);
                        b_tryfinal.push(&p);
                    }
                }
                b_bootmap.clear();
                break;
            case 2:
                b_touch = time(NULL);
                break;
            case 3:
                for (iter=b_findmap.begin();
                        iter!=b_findmap.end(); ){
                    niter = iter++;
                    bootstraper &p = *niter->second;
                    bdhtransfer *trans = p.b_transfer;
                    if (trans == NULL){
                        b_findmap.erase(niter);
                        continue;
                    }
                    delete trans;
                    p.b_transfer = NULL;
                    error = 0;
                    if (route_need_update(b_tableid)){
                        state = 1;
                    }
                }
                break;
            case 4:
                while (!b_tryfinal.empty()){
                    bootstraper &p = *b_tryfinal.front();
                    p.b_transfer = NULL;
                    b_tryfinal.pop();
                }
                break;
            case 5:
                if (!b_findmap.empty() && route_need_update(b_tableid)){
                    state = 2;
                }
                break;
            case 6:
                for (inter=b_trapmap.begin();
                        inter!=b_trapmap.end();
                        inter++){
                    delete inter->second->b_transfer;
                    delete inter->second;
                }
                b_findmap.clear();
                assert(b_bootmap.empty());
                b_trapmap.clear();
                if (getribcount()<32||b_filter.size()<4){
                    uint32_t hosts[8];
                    uint16_t ports[8];
                    uint8_t  idents[8][20];
                    int count = update_boot_nodes(b_tableid, hosts,
                            ports, idents, 8);
                    add_node_list(hosts, ports, idents, count);
                    state = 0;
                }
                break;
            case 7:
                //route_get_peers(b_dhtnet);
                if (b_tableid == 159){
                    update_all_bucket(b_dhtnet);
                    b_touch = time(NULL);
                    state = 10;
                    error = 0;
                }
                break;
            default:
                return 0;
                break;
            case 10:
                break;
            case 11:
                b_touch = time(NULL);
                dump_route_table();
                state = 10;
                break;
        }
    }

    return error;
}

int
bdhtboot::reboot()
{
    if (b_state!=8 && b_state!=0){
        return -1;
    }
    while (!b_tryfinal.empty()){
        b_tryfinal.pop();
    }
    b_filter.clear();
    b_trapmap.clear();
    b_findmap.clear();
    b_bootmap.clear();
    b_state = 0;
}
