#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <netinet/in.h>
#include <string.h>
#include <map>

#include "btkad.h"
#include "knode.h"
#include "kbucket.h"
#include "bthread.h"
#include "kfind.h"
#include "bsocket.h"
#include "btcodec.h"
#include "transfer.h"
#include "provider.h"

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
        int b_state;
        int b_concurrency;
        std::vector<kship*> b_ping_queue;
};

static bdhtnet __static_dhtnet;
static std::map<in_addr_t, kping_arg> __static_ping_args;
static ping_thread __static_ping_thread;

kfind *
btkad::find_node(char target[20])
{
    return new kfind(&__static_dhtnet, target);
}

static char __kadid[20];

int
getkadid(char kadid[20])
{
    memcpy(kadid, __kadid, 20);
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
    memcpy(__kadid, kadid, 20);
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

int
get_kbucket_index(const char kadid[20])
{
    int i;
    int index = 0;
    uint8_t orkid = 0;
    for (i=0; i<20; i++){
        orkid = __kadid[i]^kadid[i];
        if (orkid != 0){
            index = mask[orkid];
            break;
        }
    }
    index += (i<<3);
    return index;
}

ping_thread::ping_thread()
    :b_state(0), b_concurrency(0)
{
    b_ident = "ping_thread";
}

static std::vector<kitem_t> __static_ping_cache;

int
post_ping(char *buffer, int count, in_addr_t host, in_port_t port)
{
    size_t lid;
    btcodec codec;
    codec.bload(buffer, count);

    const char *kadid = codec.bget().bget("r").bget("id").c_str(&lid);
    if (kadid!=NULL && lid==20){
        kitem_t initem, oitem;
        initem.host = host;
        initem.port = port;
        initem.atime = time(NULL);
        memcpy(initem.kadid, kadid, 20);
        if (update_contact(&initem, &oitem) > 0){
            __static_ping_cache.push_back(initem);
            kping_arg arg;
            arg.host = oitem.host;
            arg.port = oitem.port;
            memcpy(arg.kadid, oitem.kadid, 20);
            __static_ping_args.insert(
                    std::make_pair(arg.host, arg));
        }
    }

    return 0;
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
                break;
            case 1:
                retry = 0;
                b_runable = false;
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
                        b_runable = true;
                        retry++;
                        state = 0;
                        delete (*iter);
                        *iter = NULL;
                    }
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
add_knode(char id[20], in_addr_t host, in_port_t port)
{
    kping_arg arg ;
    memcpy(arg.kadid, id, 20);
    arg.host = host;
    arg.port = port;
    if (__static_ping_args.find(host)
            != __static_ping_args.end()){
        return 0;
    }
    update_boot_contact(host, port);
#if 0
    __static_ping_args.insert(
            std::make_pair(host, arg));
    __static_ping_thread.bwakeup(NULL);
#endif
    return 0;
}
