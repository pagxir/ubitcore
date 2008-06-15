#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <set>

#include "bthread.h"

static int __thread_argc = 0;
static void *__thread_argv = NULL;
static j_caller __thread_caller = NULL;

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
    printf("bthread::bfailed()\n");
    return 0;
}

int
bthread::bwait()
{
    if (__thread_caller == NULL) {
        return bfailed();
    }
    return (*__thread_caller)(this,
            __thread_argc,
            __thread_argv);
}

int
bwait_cancel()
{
    __thread_caller = NULL;
    return 0;
}

int
bthread_waiter(j_caller caller, int argc, void *argv)
{
    __thread_caller = caller;
    __thread_argv = argv;
    __thread_argc = argc;
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
        if (time(NULL) < btick()){
            printf("waiting\n");
            sleep(btick() - time(NULL));
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

static int
__btime_wait(bthread *job, int t, void *argv)
{
    job->benqueue(t);
    return 0;
}

int
btime_wait(int t)
{
    if (t > time(NULL)) {
        bthread_waiter(__btime_wait, t, NULL);
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
#if 0
        printf("dup wakeup: %p %s\n", this, b_ident);
#endif
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
    benqueue(time(NULL));
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
    *timeout = THEADER->btick();
#undef THEADER
    return 0;
}

