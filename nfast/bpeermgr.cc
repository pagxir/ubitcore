/* $Id$ */
#include <stdio.h>
#include <string>
#include <assert.h>
#include <arpa/inet.h>
#include <queue>
#include <set>

#include "bthread.h"
#include "btcodec.h"
#include "bpeermgr.h"


bool
operator < (const ep1_t &a, const ep1_t &b)
{
    return a.b_host < b.b_host;
}

static std::set<ep1_t> __q_epqueue;
static std::queue<ep1_t> __q_session;
static std::queue<bthread*> __q_bqueue;

static std::queue<ep_t*> __q_live_ep;
static std::queue<bthread*> __q_live_thread;

static int
bwait_live(bthread *thread, int argc, void *argv)
{
    __q_live_thread.push(thread);
    return 0;
}


static int
bwait_queue(bthread *thread, int argc, void *argv)
{
    __q_bqueue.push(thread);
    return 0;
}

int
bdequeue(ep_t **ep)
{
    if (!__q_session.empty()){
        *ep = new ep_t(__q_session.front());
        __q_session.pop();
        return 0;
    }
    bthread_waiter(bwait_queue, 0, NULL);
    return -1;
}

static int
benqueue(ep1_t &ep)
{
    if (__q_epqueue.find(ep) == __q_epqueue.end()){
        __q_session.push(ep);
        __q_epqueue.insert(ep);
        if (!__q_bqueue.empty()){
            __q_bqueue.front()->bwakeup();
            __q_bqueue.pop();
        }
    }else{
        if ((*__q_epqueue.find(ep)).b_port != ep.b_port){
            printf("conflict address!\n");
        }
    }
    return 0;
}

int
bload_peer(const char *buffer, size_t count)
{
    int error;
    size_t size;
    btcodec codec;
    codec.bload(buffer, count);
    int interval = codec.bget().bget("interval").bget(&error);
    if (error != -1){
        printf("interval: %d\n", interval);
    }
    int done_count = codec.bget().bget("done peers").bget(&error);
    if (error != -1){
        printf("done peers: %d\n", done_count);
    }
    int num_peers = codec.bget().bget("num peers").bget(&error);
    if (error != -1){
        printf("num peers: %d\n", done_count);
    }
    int incomplete = codec.bget().bget("incomplete").bget(&error);
    if (error != -1){
        printf("incomplete: %d\n", incomplete);
    }
    const char *failure = codec.bget().bget("failure reason").c_str(&size);
    if (failure != NULL){
        std::string f(failure, size);
        printf("failure reason: %s\n", f.c_str());
        return -1;
    }
    typedef unsigned char peer_t[6];
    peer_t *peers = (peer_t*)
        codec.bget().bget("peers").c_str(&size);
    assert(peers != NULL);
    int i;
    for (i=0; i<(size/6); i++){
        ep1_t ep;
        ep.b_host = *(unsigned long *)peers[i];
        ep.b_port = htons(*(unsigned short*)(peers[i]+4));
        benqueue(ep);
    }
    printf("peer count: %d\n", __q_epqueue.size());
    return 0;
}

int
bready_push(ep_t *ep)
{
    if (__q_live_thread.empty()){
        delete ep;
        return -1;
    }

    __q_live_ep.push(ep);
    __q_live_thread.front()->bwakeup();
    __q_live_thread.pop();
    return 0;
}

int
bready_pop(ep_t *ep)
{
    if (__q_live_ep.empty()){
        bthread_waiter(bwait_live, 0, NULL);
        return -1;
    }
    *ep = *__q_live_ep.front();
    delete __q_live_ep.front();
    __q_live_ep.pop();
    return 0;
}
