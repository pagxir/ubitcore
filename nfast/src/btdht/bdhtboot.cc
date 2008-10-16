#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <queue>
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
#include "bdhtboot.h"
#include "bdhtransfer.h"
#include "bdhtroute.h"

typedef struct _npack
{
    uint8_t ident[20];
    uint8_t host[4];
    uint8_t port[2];
}npack;


bdhtboot::bdhtboot(bdhtnet *dhtnet)
{
    b_index = 0;
    b_state = 0;
    b_dhtnet = dhtnet;
}

void
bdhtboot::set_target(uint8_t target[20])
{
    memcpy(b_findtarget, target, 20);
}

void
bdhtboot::add_node(uint32_t host, uint16_t port)
{
    uint8_t ibuf[20]={0};
    netpt pt(host,port);
    ibuf[19] = b_index++;
    bdhtident dident(ibuf);
    bootstraper *traper = new bootstraper();
    traper->b_host = host;
    traper->b_port = port;
    traper->b_transfer = b_dhtnet->get_transfer();
    b_trapmap.insert(std::make_pair(pt, traper));
    b_bootmap.insert(std::make_pair(dident, traper));
}


void
bdhtboot::find_next(const void *buf, size_t len)
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
            //printf("node reenter DHT network!\n");
            continue;
        }
        traper->b_transfer = b_dhtnet->get_transfer();
    }
}

int
bdhtboot::bdocall(time_t timeout)
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
                            p.b_port, b_findtarget);
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
                            this, buffer, sizeof(buffer),
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
                            p.b_port, b_findtarget);
                    b_tryfinal.push(&p);
                }
                break;
            case 4:
                if (!b_findmap.empty()){
                    state = 1;
                }
                break;
            case 5:
                route_get_peers(b_dhtnet);
                dump_route_table();
                break;
            default:
                return 0;
                break;
        }
    }

    return error;
}
