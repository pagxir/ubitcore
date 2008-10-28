/* $Id$ */
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <queue>
#include <set>

#include "bthread.h"

bthread::bthread()
{
    b_ident = "bthread";
    b_flag = 0;
}

int
bthread::bdocall(time_t timeout)
{
    printf("Hello World\n");
    return 0;
}

int
bthread::bfailed()
{
    printf("%s::bfailed()\n", b_ident.c_str());
    return 0;
}

struct btimer
{
    bool operator()(btimer const & t1, btimer const &t2)
    {
        if (t1.btick() == t2.btick()){
            return t1.bseed()<t2.bseed();
        }
        return t1.btick()<t2.btick();
    }

    int bwait() const
    {
        if (bthread::now_time() < btick()){
            sleep(btick() - bthread::now_time());
        }
        return 0;
    }

    time_t btick() const
    {
        return tt_thread->b_tick;
    }

    int bseed() const
    {
        return tt_thread->b_seed;
    }

    int tt_flag;
    bthread *tt_thread;
};

static std::set<btimer, btimer>__q_timer;

int
btime_wait(int t)
{
    if (t > bthread::now_time()) {
        bthread::now_job()->benqueue(t);
        return -1;
    }
    return 0;
}

int
bthread::benqueue(time_t timeout)
{
    btimer __timer;
    __timer.tt_thread = this;
    __timer.tt_flag = flag();
    static int __generator = 0;
    if (b_flag&BF_ACTIVE) {
        assert(!__q_timer.empty());
        if (timeout>__timer.btick()){
            return 0;
        }
        __q_timer.erase(__timer);
    }
    b_flag |= BF_ACTIVE;
    b_tick = timeout;
    b_seed  = __generator++;
    assert(__q_timer.find(__timer) == __q_timer.end());
    __q_timer.insert(__timer);
    return 0;
}

int
bthread::bwakeup()
{
    benqueue(_tnow);
    return 0;
}

int
bthread::bpoll(bthread ** pu, time_t *timeout)
{
    bthread *item;
    if (__q_timer.empty()) {
        return -1;
    }
#define THEADER __q_timer.begin()
    while (THEADER->bwait() == -1){
        /* do nothing */
    }
    item = THEADER->tt_thread;
    item->b_flag &= ~BF_ACTIVE;
    *pu = item;
    __q_timer.erase(THEADER);
    if (timeout == NULL){
        return 0;
    }
    if (__q_timer.empty()){
        *timeout = -1;
        return 0;
    }
    _tnow = time(NULL);
    if (_tnow < THEADER->btick())
        *timeout = THEADER->btick();
    else
        *timeout = _tnow;
    _jnow = item;
#undef THEADER
    return 0;
}

time_t
bthread::now_time()
{
    return _tnow;
}

bthread*
bthread::now_job()
{
    return _jnow;
}

bthread *bthread::_jnow = NULL;
time_t bthread::_tnow = time(NULL);
