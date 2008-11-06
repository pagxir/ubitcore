/* $Id$ */
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <queue>
#include <set>

#include "bthread.h"
#include "btimerd.h"
#include "bsocket.h"

class btimerd: public bthread
{
    public:
        btimerd();
        static time_t now_time();
        int benqueue(time_t timeout);
        time_t check_timer();
        virtual int bdocall();

    public: 
        static time_t _tnow;
};


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
        time_t now = now_time();
        if (now < tt_tick){
            sleep(tt_tick - now);
        }
        return 0;
    }

    time_t tt_tick;
    bthread *tt_thread;
};

static int __btime;
static std::set<btimer, btimer>__q_timer;

btimerd::btimerd()
{
    b_ident = "btimerd";
}

static time_t next_timer;

time_t comming_timer()
{
    return next_timer;
}

time_t
btimerd::check_timer()
{
    _tnow = time(NULL);
    next_timer = _tnow;
    bthread *item = NULL;
#define THEADER __q_timer.begin()
    while (!__q_timer.empty()){
        item = THEADER->tt_thread;
        assert(THEADER->tt_tick <= item->b_tick);
        if (_tnow >= item->b_tick){
            item->bwakeup(&__btime);
        }else if (_tnow < THEADER->tt_tick){
            next_timer = THEADER->tt_tick;
            break;
        }
        __q_timer.erase(THEADER);
    }
#undef THEADER
    return next_timer;
}

int
btimerd::benqueue(time_t timeout)
{
    btimer __timer;
    bthread *thr = bthread::now_job();
    __timer.tt_tick = timeout;
    __timer.tt_thread = thr;
    thr->b_tick = timeout;
    __q_timer.insert(__timer);
    return 0;
}

int
btimerd::bdocall()
{
    printf("timed run\n");
    check_timer();
    tsleep(NULL);
    return 0;
}

time_t btimerd::_tnow;
static btimerd __timer_daemon;

int
btimercheck()
{
    __timer_daemon.bwakeup(NULL);
    return 0;
}

time_t
now_time()
{
    return btimerd::_tnow;
}

int
benqueue(time_t timeout)
{
    __timer_daemon.benqueue(timeout);
    __timer_daemon.bwakeup(NULL);
    return 0;
}

int
btime_wait(time_t t)
{
    if (t > now_time()){
        bthread::now_job()->tsleep(&__btime);
        benqueue(t);
        return -1;
    }
    return 0;
}

