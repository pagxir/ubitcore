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
    b_flag = BF_ACTIVE;
    b_runable = false;
    b_swaitident = NULL;
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

void
bthread::tsleep(void *ident, time_t timeout)
{
    assert(b_runable==true);
    b_runable = false;
    b_swaitident = ident;
    if (timeout > bthread::now_time()){
        benqueue(timeout);
    }else if (timeout > 0){
        b_runable = true;
        b_swaitident = NULL;
    }
}

struct btimer
{
    bool operator()(btimer const & t1, btimer const &t2)
    {
        if (t1.tt_tick == t2.tt_tick){
            return t1.tt_thread < t2.tt_thread;
        }
        return t1.tt_tick < t2.tt_tick;
    }

    int bwait() const
    {
        time_t now = bthread::now_time();
        if (now < tt_tick){
            sleep(tt_tick - now);
        }
        return 0;
    }

    time_t tt_tick;
    bthread *tt_thread;
};

static std::queue<bthread*> __q_running;
static std::set<btimer, btimer>__q_timer;

int
bthread::btime_wait(time_t t)
{
    static int __btime;
    tsleep(&__btime, t);
    return b_runable?0:-1;
}

int
bthread::benqueue(time_t timeout)
{
    btimer __timer;
    assert(BF_ACTIVE&b_flag);
    __timer.tt_tick = timeout;
    __timer.tt_thread = this;
    b_tick = timeout;
    __q_timer.insert(__timer);
    return 0;
}

int
bthread::bwakeup(void *wait)
{
    assert(b_swaitident==wait);
    if (b_flag & BF_ACTIVE){
        b_flag &= ~BF_ACTIVE;
        __q_running.push(this);
    }
    return 0;
}

int
bthread::bpoll(bthread ** pu, time_t *timeout)
{
    bthread *item = NULL;
    _tnow = time(NULL);
    time_t  next_timer = _tnow+36000;
#define THEADER __q_timer.begin()
    while (!__q_timer.empty()){
        item = THEADER->tt_thread;
        assert(THEADER->tt_tick <= item->b_tick);
        if (_tnow >= item->b_tick){
            if (item->b_flag&BF_ACTIVE){
                item->b_tick = next_timer;
                item->b_flag &= ~BF_ACTIVE;
                __q_running.push(item);
            }
        }else if (_tnow < THEADER->tt_tick){
            if (__q_running.empty()){
                THEADER->bwait();
                _tnow = time(NULL);
                continue;
            }
            next_timer = THEADER->tt_tick;
            break;
        }
        __q_timer.erase(THEADER);
    }
#undef THEADER
    
    if (__q_running.empty()){
        return -1;
    }
    _jnow = __q_running.front();
    __q_running.pop();
    _jnow->b_flag |= BF_ACTIVE;
    _jnow->b_runable = true;
    if (timeout != NULL){
        if (__q_running.empty()){
            *timeout = next_timer;
        }else{
            *timeout = _tnow;
        }
    }
    *pu = _jnow;
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
