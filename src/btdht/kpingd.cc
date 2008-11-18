#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <map>

#include "btkad.h"
#include "knode.h"
#include "kbucket.h"
#include "ktable.h"
#include "bthread.h"
#include "bsocket.h"
#include "btcodec.h"
#include "transfer.h"
#include "provider.h"
#include "btimerd.h"
#include "kpingd.h"

static std::map<in_addr_t, kitem_t> __static_ping_queue;

pingd::pingd()
    :b_state(0), b_concurrency(0)
{
    b_ident = "pingd";
    b_ship  =  NULL;
    b_sendmore = false;
    b_last_seen = time(NULL);
    b_swaitident = this;
}

pingd::~pingd()
{
    delete b_ship;
}

void
pingd::dump()
{
    printf("pingd:: %s\n", b_wmsg);
}


int
pingd::kping_expand(char *buffer, int count, in_addr_t addr, in_port_t port)
{
    size_t lid;
    btcodec codec;
    codec.bload(buffer, count);

    kitem_t initem;
    const char *kadid = codec.bget().bget("r").bget("id").c_str(&lid);
    if (kadid==NULL || lid!=20){
        return 0;
    }
    std::map<in_addr_t, kitem_t>::iterator iter = b_queue.find(addr);
    if (iter != b_queue.end()){
        kitem_t item = iter->second;
        if (memcmp(item.kadid, kadid, 20)!=20){
            failed_contact(&item);
        }
        b_queue.erase(iter);
    }
    initem.host = addr;
    initem.port = port;
    memcpy(initem.kadid, kadid, 20);
    update_contact(&initem, true);
#if 0
    printf("contact: %s ", idstr(kadid));
    printf(" %s:%u\n",
            inet_ntoa(*(in_addr*)&host), htons(port));
#endif
    return 0;
}

int
pingd::bdocall()
{
    int i, count;
    int state = b_state;
    char buffer[8192];
    kitem_t item;
    in_addr_t host;
    in_port_t port;
    std::map<in_addr_t, kitem_t> *ping_q;

    while (b_runable){
        b_state = state++;
        switch(b_state){
            case 0:
                if (b_ship == NULL){
                    b_ship = kship_new();
                }
                break;
            case 1:
                ping_q = &__static_ping_queue;
                while (!ping_q->empty() && b_concurrency<3){
                    item = ping_q->begin()->second;
                    b_queue.insert(*ping_q->begin());
                    ping_q->erase(ping_q->begin());
                    b_ship->ping_node(item.host, item.port);
                    b_last_seen = time(NULL);
                    b_sendmore = true;
                    b_concurrency++;
                }
                if (b_concurrency > 0){
                    /* nothing to do */
                }else if (!table_is_pingable()){
                    tsleep(NULL, "wait ping");
                    return 0;
                }else {
                    kitem_t items[_K];
                    int count = get_table_ping(items, _K);
                    for (int i=0; i<count; i++){
                        __static_ping_queue.insert(
                                std::make_pair(items[i].host, items[i]));
                    }
                    state = 1;
                }
                break;
            case 2:
                count = b_ship->get_response(buffer,
                        sizeof(buffer), &host, &port);
                while (count != -1){
                    kping_expand(buffer, count, host, port);
                    b_concurrency--;
                    count = b_ship->get_response(buffer,
                            sizeof(buffer), &host, &port);
                }
                if (b_last_seen+3<=now_time()){
                    std::map<in_addr_t, kitem_t>::iterator iter;
                    for (iter=b_queue.begin(); iter!=b_queue.end(); iter++){
                        failed_contact(&iter->second);
                    }
                    b_concurrency = 0;
                    b_queue.clear();
                    state = 1;
                }
                if (b_concurrency<_K && b_sendmore){
                    b_sendmore = false;
                    state = 1;
                }else if (b_concurrency > 0){
                    delay_resume(b_last_seen+3);
                    tsleep(this, "select");
                }else{
                    state = 1;
                }
                break;
            default:
                printf("what\n");
                break;
        }
    }
    return 0;
}

int
pingd::add_contact(in_addr_t addr, in_port_t port)
{
    kitem_t item;
    memset(item.kadid, 0, 20);
    item.host = addr;
    item.port = port;
    if (__static_ping_queue.find(addr)
            != __static_ping_queue.end()){
        return 0;
    }
    __static_ping_queue.insert(
            std::make_pair(addr, item));
    return 0;
}
