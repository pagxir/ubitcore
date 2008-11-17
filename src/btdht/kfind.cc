#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>

#include "btkad.h"
#include "btimerd.h"
#include "bthread.h"
#include "kbucket.h"
#include "kfind.h"
#include "knode.h"
#include "bsocket.h"
#include "btcodec.h"
#include "provider.h"
#include "kutils.h"
#include "transfer.h"

kfind::kfind(bdhtnet *net, const char target[20], kitem_t items[], size_t count)
{
    int i;
    kfind_t kfs;
    b_state = 0;
    b_net   = net;
    b_trim  = false;
    b_sumumery = 0;
    b_concurrency = 0;
    b_last_finding = 0;
    b_last_update = time(NULL);
    memcpy(b_target, target, 20);
    for (i=0; i<count; i++){
        kfs.ship = NULL;
        kfs.item = items[i];
        kaddist_t dist(items[i].kadid, b_target);
        b_qfind.insert(std::make_pair(dist, kfs));
    }
}

struct compat_t
{
    char ident[20];
    char host[4];
    char port[2];
};

void
kfind::kfind_expand(const char buffer[], size_t count,
        in_addr_t address, in_port_t port, const kitem_t *old)
{
    size_t len;
    kfind_t kfs;
    btcodec codec;
    codec.bload(buffer, count);

    b_sumumery++;
    kfs.ship = NULL;
    const char *vip = codec.bget().bget("r").bget("id").c_str(&len);
    if (vip == NULL || len != 20){
        failed_contact(old);
        return;
    }
    if (memcmp(vip, old->kadid, 20) != 0){
        failed_contact(old);
    }
    b_loging += idstr(vip);
    b_loging += "\n";
    b_last_finding++;
    kaddist_t dist(vip, b_target);
    kfs.item.port = port;
    kfs.item.host = address;
    memcpy(kfs.item.kadid, vip, 20);
    update_contact(&kfs.item, true);
    b_mapined.insert(std::make_pair(dist, 1));
    std::map<kaddist_t, int>::iterator backdist = b_mapined.end();
    backdist --;
    if (b_mapined.size() > 8){
        b_mapined.erase(backdist--);
    }
    if (b_mapined.size() == 8){
        b_ended =  backdist->first;
        b_trim = true;
    }
    const char *compat = codec.bget().bget("r").bget("nodes").c_str(&len);
    if (compat != NULL && (len%26)==0){
        compat_t *iter, *compated = (compat_t*)(compat+len);
        for (iter=(compat_t*)compat; iter<compated; iter++){
            kfs.item.host = *(in_addr_t*)iter->host;
            kfs.item.port = *(in_addr_t*)iter->port;
            memcpy(kfs.item.kadid, iter->ident, 20);
            update_contact(&kfs.item, false);
            dist = kaddist_t(iter->ident, b_target);
            if (b_trim && b_ended < dist){
                continue;
            }
            if (b_mapoutedkadid.find(dist)==b_mapoutedkadid.end() 
                    && b_mapoutedaddr.find(kfs.item.host)==b_mapoutedaddr.end()){
                if (b_qfind.insert(std::make_pair(dist, kfs)).second == false) {
                    /* nothing to do */
                }
            }
        }
    }
}

int
kfind::vcall()
{
    int i;
    int count;
    int error = 0;
    int state = b_state;
    char buffer[8192];
    in_addr_t host;
    in_port_t port;
    bthread  *thr = NULL;

    while (error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                break;
            case 1:
                while (b_concurrency<CONCURRENT_REQUEST){
                    if (b_qfind.empty()){
                        break;
                    }
                    if (b_trim && b_ended<b_qfind.begin()->first){
                        break;
                    }
                    kfind_t kfs = b_qfind.begin()->second;
                    kaddist_t ord = b_qfind.begin()->first;
                    b_mapoutedkadid.insert(std::make_pair(ord, 1));
                    b_mapoutedaddr.insert(std::make_pair(kfs.item.host, 1));
                    b_qfind.erase(b_qfind.begin());
                    kfs.ship = b_net->get_kship();
                    kfs.ship->find_node(kfs.item.host,
                            kfs.item.port, (uint8_t*)b_target);
                    b_outqueue.push_back(kfs);
                    b_last_update = time(NULL);
                    delay_resume(b_last_update+5);
                    b_last_finding++;
                    b_concurrency++;
                }
                if (b_concurrency == 0){
                    return b_sumumery;
                }
                break;
            case 2:
                error = -1;
                for (int i=0; i<b_outqueue.size(); i++){
                    if (b_outqueue[i].ship == NULL){
                        continue;
                    }
                    kship *ship = b_outqueue[i].ship;
                    count = ship->get_response(buffer,
                            sizeof(buffer), &host, &port);
                    if (count <= 0){
                        continue;
                    }
                    kfind_expand(buffer, count, host, port,
                            &b_outqueue[i].item);
                    b_outqueue[i].ship = NULL;
                    b_concurrency--;
                    delete ship;
                }
                thr = bthread::now_job();
                if (thr->reset_timeout()){
                    for (int i=0; i<b_outqueue.size(); i++){
                        if (b_outqueue[i].ship != NULL){
                            failed_contact(&b_outqueue[i].item);
                            delete b_outqueue[i].ship;
                            b_outqueue[i].ship = NULL;
                        }
                    }
                    b_outqueue.resize(0);
                    b_concurrency = 0;
                }
                if (b_concurrency<CONCURRENT_REQUEST && b_last_finding){
                    b_last_finding = 0;
                    error = state = 0;
                }else if (b_concurrency > 0){
                    thr->tsleep(this, "selecting");
                }else{
                    return b_sumumery;
                }
                break;
            default:
                assert(0);
                break;
        }
    }
    return error;
}

void
kfind::dump()
{
    printf("kfind result: \n%s", b_loging.c_str());
}
