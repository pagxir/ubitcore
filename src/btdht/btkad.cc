#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <map>

#include "btkad.h"
#include "knode.h"
#include "kbucket.h"
#include "bthread.h"

static std::map<bthread*, std::string> __find_node_sleep_queue;

int _find_node(char target[20])
{
    int i;
    int concurrency = 0;
    knode nodes[_K];
    int count = get_knode(target, nodes, false);
    if (count == -1){
        return -1;
    }
#if 0
    if (valid_count(nodes, _K) < 4){
        count = get_knode(nodes, _K, false);
    }
    int rds[CONCURRENT_REQUEST];
    for(;;){
        i = 0;
        while (i<_K && concurrency<CONCURRENT_REQUEST){
            if (nodes[i].requested()){
                if (nodes[i].timeout()){
                    mark_result_timeout(nodes[i]);
                }
                continue;
            }
            int rd = nodes[i]->find_node(target);
            if (rd != -1){
                nodes[i].mark_requested();
                rds[concurrency++] = rd;
            }
        }
        if (concurrency == 0){
            break;
        }
        wait_for_result(rds, concurrency, 5);
        int end = 0;
        for (i=0; i<concurrency; i++){
            if (find_finish(rds[i])){
                continue;
            }
            if (end < i){
                rds[end] = rds[i];
            }
            end++;
        }
        concurrency = end;
    }
#endif
    return 0;
}

int
btkad::find_node(char target[20])
{
    bthread *curthread = bthread::now_job();

    if (__find_node_sleep_queue.find(curthread)
            == __find_node_sleep_queue.end()){
        __find_node_sleep_queue.insert(
                std::make_pair(curthread, target));
        curthread->tsleep();
        return -1;
    }
    __find_node_sleep_queue.erase(curthread);
    return 0;
}

static char __kadid[20];

int
getkadid(char kadid[20])
{
    memcpy(kadid, __kadid, 20);
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
    uint8_t orkid;
    for (i=0; i<20; i++){
        orkid = __kadid[i]^kadid[i];
        if (orkid != 0){
            break;
        }
    }
    return (i<<3)+mask[orkid];
}

struct kping_arg{
    char kadid[20];
    in_addr_t host;
    in_port_t port;
    bool      pinging;
};

static std::map<in_addr_t, kping_arg> __static_ping_args;

class ping_thread: public bthread
{
    public:
        ping_thread();
        virtual int bdocall(time_t timeout);

    private:
        int b_state;
};

ping_thread::ping_thread()
    :b_state(0)
{
}

int
ping_thread::bdocall(time_t timeout)
{
    int state = b_state;
    while (b_runable){
        b_state = state++;
        switch(b_state){
            case 0:
                printf("ping_thread::Hello\n");
                break;
            case 1:
                printf("ping_thread::World\n");
                break;
            default:
                b_runable = false;
                break;
        }
    }
    return 0;
}

static ping_thread __static_ping_thread;

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
    __static_ping_args.insert(
            std::make_pair(host, arg));
    __static_ping_thread.bwakeup();
    return 0;
}
