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

struct kping_arg{
    char kadid[20];
    in_addr_t host;
    in_port_t port;
    kship     *ship;
};

class ping_thread: public bthread
{
    public:
        ping_thread();
        virtual int bdocall();

    private:
        time_t b_last_seen;

    private:
        int b_state;
        int b_concurrency;
        std::vector<kping_arg> b_ping_queue;
};

static bdhtnet __static_dhtnet;
static ktable __static_table;
static std::map<in_addr_t, kping_arg> __static_ping_args;
static ping_thread __static_ping_thread;

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
    uint32_t *vp = (uint32_t*)kadid;
    for (i=0; i<5; i++){
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

static uint8_t mask[256] = {
    8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

ping_thread::ping_thread()
    :b_state(0), b_concurrency(0)
{
    b_ident = "ping_thread";
    b_last_seen = time(NULL);
    b_swaitident = this;
}

static std::vector<kitem_t> __static_ping_cache;

int
post_ping(char *buffer, int count, in_addr_t host, in_port_t port, const char oldkadid[])
{
    size_t lid;
    btcodec codec;
    codec.bload(buffer, count);

    kitem_t initem;
    const char *kadid = codec.bget().bget("r").bget("id").c_str(&lid);
    if (kadid==NULL && lid!=20){
        memcpy(initem.kadid, oldkadid, 20);
        failed_contact(&initem);
        return 0;
    }
    if (memcmp(oldkadid, kadid, 20) != 0){
        memcpy(initem.kadid, oldkadid, 20);
        failed_contact(&initem);
    }
    initem.host = host;
    initem.port = port;
    initem.atime = time(NULL);
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
    if (__static_table.need_ping()){
        __static_ping_thread.bwakeup(&__static_ping_thread);
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
ping_thread::bdocall()
{
    int retry = 0;
    int state = b_state;
    char buffer[8192];
    std::vector<kping_arg>::iterator iter;
    std::map<in_addr_t, kping_arg> *args;
    while (b_runable){
        b_state = state++;
        switch(b_state){
            case 0:
                args = &__static_ping_args;
                while (!args->empty() && b_concurrency<_K){
                    kping_arg arg = args->begin()->second;
                    args->erase(args->begin());
                    kship *transfer = __static_dhtnet.get_kship();
                    arg.ship = transfer;
                    b_ping_queue.push_back(arg);
                    transfer->ping_node(arg.host, arg.port);
                    b_concurrency++;
                }
                if (b_concurrency == 0){
                    if (!__static_table.need_ping()){
                        tsleep(this, "wait ping");
                        return 0;
                    }
                    kitem_t item;
                    if (-1 != __static_table.get_ping(&item)){
                        kping_arg arg ;
                        memcpy(arg.kadid, item.kadid, 20);
                        arg.host = item.host;
                        arg.port = item.port;
                        if (__static_ping_args.find(item.host)
                                == __static_ping_args.end()){
                            __static_ping_args.insert(
                                    std::make_pair(item.host, arg));
                        }
                    }
                    state = 0;
                }else{
                    b_last_seen = time(NULL);
                    benqueue(this, b_last_seen+5);
                }
                break;
            case 1:
                tsleep(this, "select");
                for (iter=b_ping_queue.begin();
                        iter!=b_ping_queue.end(); iter++){
                    in_addr_t host;
                    in_port_t port;
                    if ((*iter).ship == NULL){
                        continue;
                    }
                    int count = (*iter).ship->get_response(buffer,
                            sizeof(buffer), &host, &port);
                    if (count > 0){
                        post_ping(buffer, count, host, port, (*iter).kadid);
                        b_concurrency--;
                        bwakeup(this);
                        state = 0;
                        delete (*iter).ship;
                        (*iter).ship = NULL;
                    }
                }
                if (b_last_seen+5 <= time(NULL)){
                    b_concurrency = 0;
                    for (int i=0; i<b_ping_queue.size(); i++){
                        if (b_ping_queue[i].ship != NULL){
                            kitem_t item;
                            memcpy(item.kadid, b_ping_queue[i].kadid, 20);
                            item.host = b_ping_queue[i].host;
                            item.port = b_ping_queue[i].port;
                            failed_contact(&item);
                            delete b_ping_queue[i].ship;
                        }
                    }
                    b_ping_queue.clear();
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
    kping_arg arg ;
    memset(arg.kadid, 0, 20);
    arg.host = addr;
    arg.port = port;
    if (__static_ping_args.find(addr)
            != __static_ping_args.end()){
        return 0;
    }
    __static_ping_args.insert(
            std::make_pair(addr, arg));
    __static_ping_thread.bwakeup(&__static_ping_thread);
#endif
    return 0;
}

int
find_nodes(const char *target, kitem_t items[_K], bool valid)
{
    return __static_table.find_nodes(target, items);
}

static refreshthread *__static_refresh[160];
int
refresh_routing_table()
{
    int i;
    for (i=0; i<__static_table.size(); i++){
        if (__static_refresh[i] == NULL){
            __static_refresh[i] = new refreshthread(i);
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

class checkthread: public bthread
{
    public:
        checkthread();
        virtual int bdocall();

    private:
        int b_state;
        time_t b_random;
        time_t b_start_time;
        time_t b_last_refresh;
};

checkthread::checkthread()
{
    b_state = 0;
    b_random = 60;
    b_start_time = now_time();
    b_last_refresh = now_time();
    b_ident = "checkthread";
}

int
checkthread::bdocall()
{
    int state = b_state;
    while (b_runable){
        b_state = state++;
        switch(b_state)
        {
            case 0:
                btime_wait(b_start_time+120);
                if (now_time() > b_last_refresh+b_random){
                    b_random = (time_t)((60*15*0.9)+(rand()%(60*15))/5);
                    b_last_refresh = now_time();
                    printf("randomize: %u\n", b_random);
                    refresh_routing_table();
                }
                break;
            case 1:
                dump_routing_table();
                b_start_time = now_time();
                state = 0;
                break;
            default:
                assert(0);
                break;
        }
    }
    return 0;
}

class boothread: public bthread
{
    public:
        boothread();
        virtual int bdocall();

    private:
        int b_state;
        time_t b_random;
        time_t b_start_time;
        kfind  *b_find;
        bool   b_must_validate;
};

boothread::boothread()
{
    b_find = NULL;
    b_state = 0;
    b_must_validate = false;
    b_start_time = now_time();
    b_ident = "boothread";
}

int
boothread::bdocall()
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
                count = find_nodes(bootid, items, b_must_validate);
                if (count == 0){
                    if (b_must_validate == true){
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
                if (error == -1){
                    state = 2;
                }
                break;
            case 3:
                b_must_validate = (error>4);
                break;
            case 4:
                printf("DHT Boot Ended\n");
                refresh_routing_table();
                if (get_table_size() > 4){
                    btime_wait(b_start_time+b_random);
                }
                break;
            case 5:
                printf("DHT: Refresh Boot!\n");
                delete b_find;
                state = 0;
                break;
            default:
                assert(0);
                break;
        }
    }
}

static checkthread __static_checkthread;
static boothread __static_boothread;

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
    __static_boothread.bwakeup(NULL);
    __static_checkthread.bwakeup(NULL);
    return 0;
}

int
get_table_size()
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
