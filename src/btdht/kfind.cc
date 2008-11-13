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

struct kfind_arg{
    char kadid[20];
    in_addr_t host;
    in_port_t port;
    time_t    age;
    time_t    atime;
    int       failed;
    kship     *ship;
};

kfind::kfind(bdhtnet *net, const char target[20], kitem_t items[], size_t count)
{
    int i;
    b_state = 0;
    b_net   = net;
    b_trim  = false;
    b_concurrency = 0;
    b_sumumery = 0;
    memcpy(b_target, target, 20);
    for (i=0; i<count; i++){
        kfind_arg *arg = new kfind_arg;
        arg->host = items[i].host;
        arg->port = items[i].port;
        memcpy(arg->kadid, items[i].kadid, 20);
        kaddist_t dist(arg->kadid, b_target);
        b_kfind_queue.insert(
                std::make_pair(dist, arg));
    }
}

struct compat_t
{
    char ident[20];
    char host[4];
    char port[2];
};

void
kfind::decode_packet(const char buffer[], size_t count,
        in_addr_t address, in_port_t port, const char kadid[20])
{
    size_t len;
    kitem_t in, out;
    btcodec codec;
    codec.bload(buffer, count);

    b_sumumery++;
    const char *vip = codec.bget().bget("r").bget("id").c_str(&len);
    if (vip == NULL || len != 20){
        memcpy(in.kadid, kadid, 20);
        in.host = address;
        in.port = port;
        failed_contact(&in);
        return;
    }
    if (memcmp(vip, kadid, 20) != 0){
        memcpy(in.kadid, kadid, 20);
        in.host = address;
        in.port = port;
        failed_contact(&in);
    }
    memcpy(in.kadid, vip, 20);
    in.host = address;
    in.port = port;
    update_contact(&in, &out, true);
    kaddist_t dist(vip, b_target);
    b_kfind_ined.insert(std::make_pair(dist, 1));
    std::map<kaddist_t, int>::iterator backdist = b_kfind_ined.end();
    backdist --;
    if (b_kfind_ined.size() > 8){
        b_kfind_ined.erase(backdist--);
    }
    if (b_kfind_ined.size() == 8){
        b_trim = true;
        b_ended =  backdist->first;
    }
    const char *compat = codec.bget().bget("r").bget("nodes").c_str(&len);
    if (compat != NULL && (len%26)==0){
        compat_t *iter = (compat_t*)compat;
        compat_t *compated = (compat_t*)(compat+len);
        for (iter; iter<compated; iter++){
            memcpy(in.kadid, iter->ident, 20);
#if 0
            printf("find node result: %s\n", idstr(in.kadid));
#endif
            memcpy(&in.host, &iter->host, sizeof(in_addr_t));
            memcpy(&in.port, &iter->port, sizeof(in_port_t));
            update_contact(&in, &out, false);
#if 0
            printf("kfind: %s:%d\n", 
                    inet_ntoa(*(in_addr*)&in.host), ntohs(in.port));
#endif
            kaddist_t dist(in.kadid, b_target);
            if (b_kfind_outed.find(dist) != b_kfind_outed.end()){
                continue;
            }
            if (b_trim && b_ended < dist){
                continue;
            }
            kfind_arg *arg = new kfind_arg;
            arg->host = in.host;
            arg->port = in.port;
            memcpy(arg->kadid, in.kadid, 20);
            if (b_kfind_queue.insert(
                    std::make_pair(dist, arg)).second == false) {
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
    std::vector<kfind_arg*>::iterator iter;

    while (error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                break;
            case 1:
                while (b_concurrency<CONCURRENT_REQUEST){
                    if (b_kfind_queue.empty()){
                        break;
                    }
                    if (b_trim && b_ended<b_kfind_queue.begin()->first){
                        break;
                    }
                    kfind_arg *arg = b_kfind_queue.begin()->second;
                    b_kfind_outed.insert(
                            std::make_pair(b_kfind_queue.begin()->first, 1));
                    b_kfind_queue.erase(b_kfind_queue.begin());
                    arg->ship = b_net->get_kship();
                    arg->ship->find_node(arg->host, arg->port,
                            (uint8_t*)b_target);
                    b_kfind_out.push_back(arg);
                    b_concurrency++;
                }
                if (b_concurrency == 0){
                    //printf("summery: %d\n", b_sumumery);
                    return b_sumumery;
                }
                b_last_update = time(NULL);
                thr = bthread::now_job();
                thr->tsleep(thr);
                break;
            case 2:
                if (b_last_update+5 > now_time()){
                    error = -1;
                    thr = bthread::now_job();
                    benqueue(thr, b_last_update+5);
                }else{
                    for (iter = b_kfind_out.begin(); 
                            iter != b_kfind_out.end();
                            iter++){
                        kitem_t in;
                        kfind_arg *ping = *iter;
                        memcpy(in.kadid, ping->kadid, 20);
                        in.host = ping->host;
                        in.port = ping->port;
                        failed_contact(&in);
                        delete (*iter)->ship;
                        delete (*iter);
                    }
                    b_kfind_out.resize(0);
                    b_concurrency = 0;
                    error = 0;
                    state = 1;
                    break;
                }
                for (iter = b_kfind_out.begin(); 
                        iter != b_kfind_out.end();
                        iter++){
                    if ((*iter)->ship == NULL){
                        continue;
                    }
                    kship *ship = (*iter)->ship;
                    count = ship->get_response(buffer,
                                sizeof(buffer), &host, &port);
                    if (count > 0){
                        if (b_concurrency > 0){
                            b_concurrency--;
                        }
                        decode_packet(buffer, count, host, port, (*iter)->kadid);
                        delete ship;
                        (*iter)->ship = NULL;
                        error = 0;
                        state = 1;
                    }else{
#if 0
                    if (b_trim && b_ended<b_kfind_queue.begin()->first){
                        printf("empty0\n");
                        break;
                    }
#endif
                    }
                }
                break;
            default:
                break;
        }
    }
    return error;
}
