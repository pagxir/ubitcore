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

static std::map<in_addr_t, kping_t> __static_ping_queue;

pingd::pingd()
    :b_state(0), b_concurrency(0)
{
    b_ident = "pingd";
    b_sendmore = false;
    b_last_seen = time(NULL);
    b_swaitident = this;
}

void
pingd::dump()
{
    printf("pingd:: %s\n", b_wmsg);
}


static std::vector<kitem_t> __static_ping_cache;

int
kping_expand(char *buffer, int count, in_addr_t addr,
        in_port_t port, kitem_t *old)
{
    size_t lid;
    btcodec codec;
    codec.bload(buffer, count);

    kitem_t initem;
    const char *kadid = codec.bget().bget("r").bget("id").c_str(&lid);
    if (kadid==NULL || lid!=20){
        failed_contact(old);
        return 0;
    }
    if (memcmp(old->kadid, kadid, 20) != 0){
        failed_contact(old);
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
    int i;
    int state = b_state;
    char buffer[8192];
    kping_t ping_struct;
    std::map<in_addr_t, kping_t> *ping_q;

    while (b_runable){
        b_state = state++;
        switch(b_state){
            case 0:
                ping_q = &__static_ping_queue;
                while (!ping_q->empty() && b_concurrency<_K){
                    ping_struct = ping_q->begin()->second;
                    ping_q->erase(ping_q->begin());
                    kship *transfer = kship_new();
                    ping_struct.ship = transfer;
                    b_queue.push_back(ping_struct);
                    transfer->ping_node(ping_struct.item.host,
                            ping_struct.item.port);
                    b_last_seen = time(NULL);
                    b_sendmore = true;
                    b_concurrency++;
                }
                if (b_concurrency > 0){
                    delay_resume(b_last_seen+3);
                }else if (!table_is_pingable()){
                    b_text_satus = "wait ping";
                    tsleep(NULL, "wait ping");
                    return 0;
                }else {
                    kitem_t items[_K];
                    int count = get_table_ping(items, _K);
                    for (int i=0; i<count; i++){
                        ping_struct.item = items[i];
                        __static_ping_queue.insert(
                                std::make_pair(items[i].host, ping_struct));
                    }
                    state = 0;
                }
                break;
            case 1:
                for (i=0; i<b_queue.size(); i++){
                    int count = -1;
                    in_addr_t host;
                    in_port_t port;
                    if (b_queue[i].ship == NULL){
                        continue;
                    }
                    count = b_queue[i].ship->get_response(buffer,
                            sizeof(buffer), &host, &port);
                    if (count == -1){
                        continue;
                    }
                    kping_expand(buffer, count, host, port, &b_queue[i].item);
                    delete b_queue[i].ship;
                    b_queue[i].ship = NULL;
                    b_concurrency--;
                }
                if (reset_timeout()){
                    for (int i=0; i<b_queue.size(); i++){
                        if (b_queue[i].ship != NULL){
                            failed_contact(&b_queue[i].item);
                            delete b_queue[i].ship;
                            b_queue[i].ship = NULL;
                        }
                    }
                    b_concurrency = state = 0;
                    b_queue.clear();
                }
                if (b_concurrency<_K && b_sendmore){
                    b_sendmore = false;
                    state = 0;
                }else if (b_concurrency > 0){
                    tsleep(this, "select");
                }else{
                    state = 0;
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
    kping_t ping_struct ;
    memset(ping_struct.item.kadid, 0, 20);
    ping_struct.item.host = addr;
    ping_struct.item.port = port;
    if (__static_ping_queue.find(addr)
            != __static_ping_queue.end()){
        return 0;
    }
    __static_ping_queue.insert(
            std::make_pair(addr, ping_struct));
    return 0;
}
