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
    b_state = 0;
    b_trim  = false;
    b_sumumery = 0;
    b_concurrency = 0;
    b_last_finding = 0;
    b_last_update = time(NULL);
    memcpy(b_target, target, 20);
    for (i=0; i<count; i++){
        kaddist_t dist(items[i].kadid, b_target);
        if (b_mapoutedkadid.insert(std::make_pair(dist, 1)).second){
            if (b_mapoutedaddr.insert(std::make_pair(items[i].host, 1)).second){
                b_qfind.insert(std::make_pair(dist, items[i]));
            }
        }
        char buff[203];
        sprintf(buff, "%s:%d\n",
                inet_ntoa(*(in_addr*)&items[i].host),
                htons(items[i].port));
        b_loging +=  buff;
    }
    b_ship = net->get_kship();
}

struct compat_t
{
    char ident[20];
    char host[4];
    char port[2];
};

void
kfind::kfind_expand(const char buffer[], size_t count,
        in_addr_t address, in_port_t port)
{
    size_t len;
    kitem_t item;
    btcodec codec;
    codec.bload(buffer, count);

    b_sumumery++;
    const char *vip = codec.bget().bget("r").bget("id").c_str(&len);
    if (vip == NULL || len != 20){
        return;
    }
    b_loging += idstr(vip);
    b_loging += "\n";
    b_last_finding++;
    kaddist_t dist(vip, b_target);
    b_outqueue.erase(dist);
    item.port = port;
    item.host = address;
    memcpy(item.kadid, vip, 20);
    update_contact(&item, true);
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
            item.host = *(in_addr_t*)iter->host;
            item.port = *(in_addr_t*)iter->port;
            memcpy(item.kadid, iter->ident, 20);
            update_contact(&item, false);
            dist = kaddist_t(iter->ident, b_target);
            if (b_trim && b_ended < dist){
                continue;
            }
            if (b_mapoutedkadid.insert(std::make_pair(dist, 1)).second){
                if (b_mapoutedaddr.insert(std::make_pair(item.host, 1)).second){
                    b_qfind.insert(std::make_pair(dist, item));
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
                    kitem_t item = b_qfind.begin()->second;
                    b_outqueue.insert(*b_qfind.begin());
                    b_qfind.erase(b_qfind.begin());
                    b_ship->find_node(item.host,
                            item.port, (uint8_t*)b_target);
                    b_last_update = time(NULL);
                    b_last_finding++;
                    b_concurrency++;
                }
                if (b_concurrency == 0){
                    return b_sumumery;
                }
                break;
            case 2:
                error = -1;
                count = b_ship->get_response(buffer, sizeof(buffer), &host, &port);
                while (count > 0){
                    kfind_expand(buffer, count, host, port);
                    count = b_ship->get_response(buffer, sizeof(buffer), &host, &port);
                    b_concurrency--;
                }
                thr = bthread::now_job();
                if (b_last_update+5>=now_time()){
                    std::map<kaddist_t, kitem_t>::iterator iter;
                    for (iter=b_outqueue.begin(); iter!=b_outqueue.end(); iter++){
                        failed_contact(&iter->second);
                    }
                    b_outqueue.clear();
                    b_concurrency = 0;
                }
                if (b_concurrency<CONCURRENT_REQUEST && b_last_finding){
                    b_last_finding = 0;
                    error = state = 0;
                }else if (b_concurrency > 0){
                    delay_resume(b_last_update+5);
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

kfind::~kfind()
{
    delete b_ship;
}

void
kfind::dump()
{
    printf("kfind result: \n%s", b_loging.c_str());
}
