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
        time_t check_timer();
        virtual int bdocall();
        int benqueue(void *ident, time_t timeout);

    public: 
        static time_t now_time();
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

    time_t tt_tick;
    void  *tt_ident;
    bthread *tt_thread;
};

static std::set<btimer, btimer>__q_timer;

btimerd::btimerd()
{
    b_ident = "btimerd";
}

static time_t __comming_time = time(NULL);

time_t comming_time()
{
    time_t now = now_time();
    if (!__q_timer.empty()){
        now = __q_timer.begin()->tt_tick;
    }
    return now;
}

time_t
btimerd::check_timer()
{
    _tnow = time(NULL);
    __comming_time = _tnow;
    bthread *item = NULL;
#define THEADER __q_timer.begin()
    while (!__q_timer.empty()){
        item = THEADER->tt_thread;
        assert(THEADER->tt_tick <= item->b_tick);
        if (_tnow >= item->b_tick){
            item->bwakeup(THEADER->tt_ident);
        }else if (_tnow < THEADER->tt_tick){
            __comming_time = THEADER->tt_tick;
            break;
        }
        __q_timer.erase(THEADER);
    }
#undef THEADER
    return __comming_time;
}

int
btimerd::benqueue(void *ident, time_t timeout)
{
    btimer __timer;
    bthread *thr = bthread::now_job();
    __timer.tt_tick = timeout;
    __timer.tt_ident = ident;
    __timer.tt_thread = thr;
    thr->b_tick = timeout;
    __q_timer.insert(__timer);
    return 0;
}

int
btimerd::bdocall()
{
    check_timer();
    return 0;
}

time_t btimerd::_tnow = time(NULL);
static btimerd __timer_daemon;

int
btimerdrun()
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
delay_resume(void *ident, time_t timeout)
{
    __timer_daemon.benqueue(ident, timeout);
    return 0;
}

int
btime_wait(time_t t)
{
    static int _twait;
    if (t <= now_time()){
        return 0;
    }
    bthread *thr = bthread::now_job();
    thr->tsleep(&_twait, "time wait");
    __timer_daemon.benqueue(&_twait, t);
    return -1;
}

