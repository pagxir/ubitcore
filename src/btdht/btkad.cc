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
#include "kfind.h"
#include "bsocket.h"
#include "btcodec.h"
#include "transfer.h"
#include "refresh.h"
#include "provider.h"
#include "btimerd.h"

struct kping_t{
    kitem_t    item;
    kship     *ship;
};

class pingd: public bthread
{
    public:
        pingd();
        virtual int bdocall();

    private:
        time_t b_last_seen;

    private:
        int b_state;
        int b_concurrency;
        std::vector<kping_t> b_queue;
};

static pingd __static_pingd;
static ktable __static_table;
static bdhtnet __static_dhtnet;
static std::map<in_addr_t, kping_t> __static_ping_queue;

kfind *
kfind_new(char target[20], kitem_t items[], size_t count)
{
    return new kfind(&__static_dhtnet, target, items, count);
}

int
getkadid(char kadid[20])
{
    __static_table.getkadid(kadid);
    return 0;
}

int
genkadid(char kadid[20])
{
    int i;
    /* on windows, this rand() is limited to 65535 */
    uint16_t *vp = (uint16_t*)kadid;
    for (i=0; i<10; i++){
        vp[i] = rand();
    }
    return 0;
}

int
setkadid(const char kadid[20])
{
    printf("setkadid: %s\n", idstr(kadid));
    __static_table.setkadid(kadid);
    return 0;
}

pingd::pingd()
    :b_state(0), b_concurrency(0)
{
    b_ident = "pingd";
    b_last_seen = time(NULL);
    b_swaitident = this;
}

static std::vector<kitem_t> __static_ping_cache;

int
kping_expand(char *buffer, int count, in_addr_t addr,
        in_port_t port, kping_t *ping_struct)
{
    size_t lid;
    btcodec codec;
    codec.bload(buffer, count);

    kitem_t initem;
    const char *kadid = codec.bget().bget("r").bget("id").c_str(&lid);
    if (kadid==NULL || lid!=20){
        failed_contact(&ping_struct->item);
        return 0;
    }
    if (memcmp(ping_struct->item.kadid, kadid, 20) != 0){
        failed_contact(&ping_struct->item);
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

static std::map<in_addr_t, kitem_t> __static_active;

int
update_contact(const kitem_t *in, bool contacted)
{
    if (contacted == true){
        __static_active.insert(
                std::make_pair(in->host, *in));
    }
    int retval =__static_table.insert_node(in, contacted);
    if (__static_table.pingable()){
        __static_pingd.bwakeup(&__static_pingd);
    }
    return retval;
}

int
failed_contact(const kitem_t *in)
{
    __static_active.erase(in->host);
    return __static_table.failed_contact(in);
}

int
pingd::bdocall()
{
    int retry = 0;
    int state = b_state;
    char buffer[8192];
    std::vector<kping_t>::iterator iter;
    std::map<in_addr_t, kping_t> *ping_q;
    while (b_runable){
        b_state = state++;
        switch(b_state){
            case 0:
                ping_q = &__static_ping_queue;
                while (!ping_q->empty() && b_concurrency<_K){
                    kping_t ping_struct = ping_q->begin()->second;
                    ping_q->erase(ping_q->begin());
                    kship *transfer = __static_dhtnet.get_kship();
                    ping_struct.ship = transfer;
                    b_queue.push_back(ping_struct);
                    transfer->ping_node(ping_struct.item.host,
                            ping_struct.item.port);
                    b_concurrency++;
                }
                if (b_concurrency == 0){
                    if (!__static_table.pingable()){
                        tsleep(this, "wait ping");
                        return 0;
                    }
                    kping_t arg ;
                    if (-1 != __static_table.get_ping(&arg.item)){
                        if (__static_ping_queue.find(arg.item.host)
                                == __static_ping_queue.end()){
                            __static_ping_queue.insert(
                                    std::make_pair(arg.item.host, arg));
                        }
                    }
                    state = 0;
                }else{
                    b_last_seen = time(NULL);
                    delay_resume(this, b_last_seen+5);
                }
                break;
            case 1:
                tsleep(this, "select");
                for (iter=b_queue.begin();
                        iter!=b_queue.end(); iter++){
                    in_addr_t host;
                    in_port_t port;
                    if ((*iter).ship == NULL){
                        continue;
                    }
                    int count = (*iter).ship->get_response(buffer,
                            sizeof(buffer), &host, &port);
                    if (count > 0){
                        kping_expand(buffer, count, host, port,
                                &(*iter));
                        b_concurrency--;
                        bwakeup(this);
                        state = 0;
                        delete (*iter).ship;
                        (*iter).ship = NULL;
                    }
                }
                if (b_last_seen+5 <= time(NULL)){
                    b_concurrency = 0;
                    for (int i=0; i<b_queue.size(); i++){
                        if (b_queue[i].ship != NULL){
                            failed_contact(&b_queue[i].item);
                            delete b_queue[i].ship;
                            b_queue[i].ship = NULL;
                        }
                    }
                    b_queue.clear();
                    bwakeup(this);
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


static int __boot_count = 0;
static kitem_t __boot_contacts[8];

int
add_boot_contact(in_addr_t addr, in_port_t port)
{
    if (__boot_count < 8){
        int idx = __boot_count++;
        __boot_contacts[idx].host = addr;
        __boot_contacts[idx].port = port;
        return 0;
    }
#if 1
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
    __static_pingd.bwakeup(&__static_pingd);
#endif
    return 0;
}

int
find_nodes(const char *target, kitem_t items[_K], bool valid)
{
    return __static_table.find_nodes(target, items, valid);
}

static refreshd *__static_refresh[160];
int
refresh_routing_table()
{
    int i;
    for (i=0; i<__static_table.size(); i++){
        if (__static_refresh[i] == NULL){
            __static_refresh[i] = new refreshd(i);
        }
        __static_refresh[i]->bwakeup(NULL);
    }
    return 0;
}

void
dump_routing_table()
{
    __static_table.dump();
#if 1
    printf("dump active node:\n");
    std::map<in_addr_t, kitem_t>::iterator iter;
    for (iter = __static_active.begin();
            iter != __static_active.end(); iter++){
        in_addr_t addr = iter->second.host;
        in_port_t port = iter->second.port;
        printf("%s: %s:%d\n", idstr(iter->second.kadid),
                inet_ntoa(*(in_addr*)&addr), htons(port));
    }
#endif
}

class checkerd: public bthread
{
    public:
        checkerd();
        virtual int bdocall();

    private:
        int b_state;
        time_t b_last_show;
        time_t b_next_refresh;
};

checkerd::checkerd()
{
    b_state = 0;
    b_ident = "checkerd";
    b_last_show = now_time();
    b_next_refresh = now_time();
}

int
checkerd::bdocall()
{
    int state = b_state;
    while (b_runable){
        b_state = state++;
        switch(b_state)
        {
            case 0:
                btime_wait(b_last_show+90);
                if (now_time() > b_next_refresh){
                    time_t random = (time_t)((60*15*0.9)+(rand()%(60*15))/5);
                    b_next_refresh = now_time()+random;
                    printf("randomize: %u\n", random);
                    refresh_routing_table();
                }
                break;
            case 1:
                dump_routing_table();
                b_last_show = now_time();
                state = 0;
                break;
            default:
                assert(0);
                break;
        }
    }
    return 0;
}

class bootupd: public bthread
{
    public:
        bootupd();
        virtual int bdocall();

    private:
        int b_state;
        kfind  *b_find;
        bool   b_refresh;
        bool   b_usevalid;

    private:
        time_t b_random;
        time_t b_start_time;
};

bootupd::bootupd()
{
    b_find = 0;
    b_state = 0;
    b_ident = "bootupd";
    b_refresh = true;
    b_usevalid = false;
    b_start_time = now_time();
}

int
bootupd::bdocall()
{
    int count;
    int error = -1;
    int state = b_state;
    char bootid[20];
    kitem_t items[8];
    while (b_runable){
        b_state = state++;
        switch(b_state)
        {
            case 0:
                getkadid(bootid);
                bootid[19]^=0x1;
                count = find_nodes(bootid, items, b_usevalid);
                if (count == 0){
                    if (b_usevalid == true){
                        count = find_nodes(bootid, items, false);
                    }
                    if (count == 0){
                        count = std::min(__boot_count, 8);
                        memcpy(items, __boot_contacts, count*sizeof(kitem_t));
                    }
                }
                if (count == 0){
                    tsleep(NULL, "exit");
                    return 0;
                }
                b_find = kfind_new(bootid, items, count);
                break;
            case 1:
                b_random = (time_t)((60*15*0.9)+(rand()%(60*15))/5);
                b_start_time = now_time();
                break;
            case 2:
                error = b_find->vcall();
                break;
            case 3:
                b_usevalid = (error<4);
                break;
            case 4:
                if (size_of_table() > 4){
                    if (b_refresh == true){
                        refresh_routing_table();
                        b_refresh = false;
                    }
                    btime_wait(b_start_time+b_random);
                }
                break;
            case 5:
                delete b_find;
                state = 0;
                break;
            default:
                assert(0);
                break;
        }
    }
}

static bootupd __static_bootupd;
static checkerd __static_checkerd;

int
bdhtnet_start()
{
    static int __call_once_only = 0;

    if (__call_once_only++ > 0){
        return -1;
    }
    char bootid[20];
    genkadid(bootid);
    setkadid(bootid);
    extern char __ping_node[];
    getkadid(&__ping_node[12]);
    extern char __find_node[];
    getkadid(&__find_node[12]);
    extern char __get_peers[];
    getkadid(&__get_peers[12]);
    __static_bootupd.bwakeup(NULL);
    __static_checkerd.bwakeup(NULL);
    return 0;
}

int
size_of_table()
{
    return __static_table.size();
}

int
size_of_bucket(int index)
{
    return __static_table.size_of_bucket(index);
}

int
bit1_index_of(const char kadid[20])
{
    return __static_table.bit1_index_of(kadid);
}
