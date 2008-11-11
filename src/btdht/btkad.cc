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
    time_t    age;
    time_t    atime;
    int       failed;
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
        std::vector<kship*> b_ping_queue;
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
post_ping(char *buffer, int count, in_addr_t host, in_port_t port)
{
    size_t lid;
    btcodec codec;
    codec.bload(buffer, count);

    printf("post ping called\n");
    const char *kadid = codec.bget().bget("r").bget("id").c_str(&lid);
    if (kadid==NULL && lid!=20){
        printf("bad kadid\n");
        return 0;
    }
    kitem_t initem, oitem;
    initem.host = host;
    initem.port = port;
    initem.atime = time(NULL);
    memcpy(initem.kadid, kadid, 20);
    if (__static_table.insert_node(&initem, &oitem, true) > 0){
        __static_ping_cache.push_back(initem);
        kping_arg arg;
        arg.host = oitem.host;
        arg.port = oitem.port;
        memcpy(arg.kadid, oitem.kadid, 20);
        __static_ping_args.insert(
                std::make_pair(arg.host, arg));
    }
    printf("contact: %s ", idstr(kadid));
    printf("inet: %s:%u\n",
            inet_ntoa(*(in_addr*)&host), htons(port));
    __static_table.dump();
    return 0;
}

int
update_contact(const kitem_t *in, kitem_t *out, bool contacted)
{
    printf("kitem: update contact\n");
    return __static_table.insert_node(in, out, contacted);
}

int
failed_contact(const kitem_t *in)
{
    return __static_table.invalid_node(in);
}

int
ping_thread::bdocall()
{
    int retry = 0;
    int state = b_state;
    char buffer[8192];
    std::vector<kship*>::iterator iter;
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
                    b_ping_queue.push_back(transfer);
                    transfer->ping_node(arg.host, arg.port);
                    b_concurrency++;
                }
                if (b_concurrency == 0){
                    tsleep(this);
                    return 0;
                }
                b_last_seen = time(NULL);
                benqueue(this, b_last_seen+5);
                break;
            case 1:
                tsleep(this);
                for (iter=b_ping_queue.begin();
                        iter!=b_ping_queue.end(); iter++){
                    in_addr_t host;
                    in_port_t port;
                    if (*iter == NULL){
                        continue;
                    }
                    int count = (*iter)->get_response(buffer,
                            sizeof(buffer), &host, &port);
                    if (count > 0){
                        post_ping(buffer, count, host, port);
                        b_concurrency--;
                        bwakeup(this);
                        state = 0;
                        delete (*iter);
                        *iter = NULL;
                    }
                }
                if (b_last_seen+5 <= time(NULL)){
                    b_concurrency = 0;
                    printf("ping queue: %d\n",
                            __static_ping_args.size());
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
                btime_wait(b_start_time+60);
                if (now_time() > b_last_refresh+b_random){
                    b_random = (60*15*0.9)+(rand()%(60*15))/5;
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
};

boothread::boothread()
{
    b_find = NULL;
    b_state = 0;
    b_start_time = now_time();
    b_ident = "boothread";
}

int
boothread::bdocall()
{
    int count;
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
                count = find_nodes(bootid, items, true);
                if (count == 0){
                    count = std::min(__boot_count, 8);
                    memcpy(items, __boot_contacts, count);
                }
                b_find = kfind_new(bootid, items, count);
                break;
            case 1:
                b_random = (60*15*0.9)+(rand()%(60*15))/5;
                b_start_time = now_time();
                break;
            case 2:
                if (b_find->vcall() == -1){
                    state = 2;
                }
                break;
            case 3:
                printf("DHT Boot Ended\n");
                dump_routing_table();
                refresh_routing_table();
                if (get_table_size() < 4){
                    btime_wait(b_start_time+b_random);
                }
                break;
            case 4:
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
