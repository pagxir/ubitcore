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
        if (t1.tt_tick == t2.tt_tick){
            return t1.tt_seed<t2.tt_seed;
        }
        return t1.tt_tick<t2.tt_tick;
    }

    int bwait() const
    {
        if (time(NULL) < tt_tick){
            printf("waiting\n");
            sleep(tt_tick - time(NULL));
        }
        return 0;
    }

    int tt_flag;
    int tt_seed;
    time_t tt_tick;
    bthread *tt_thread;
};

static std::set<btimer, btimer>__q_timer;

static int
__btime_wait(bthread *job, int t, void *argv)
{
    static int __generator = 0;
    btimer __timer;
    if (t < time(NULL)) {
        job->bwakeup();
        return 0;
    }
    __timer.tt_seed = __generator++;
    __timer.tt_tick = t;
    __timer.tt_thread = job;
    __timer.tt_flag = job->flag();
    assert(__q_timer.find(__timer) == __q_timer.end());
    __q_timer.insert(__timer);
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
bthread::bwakeup()
{
    if (b_flag&BF_ACTIVE) {
        return 0;
    }
    b_flag |= BF_ACTIVE;
    b_flag &= ~(BF_IDLE);
    __btime_wait(this, time(NULL), NULL);
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
    *timeout = THEADER->tt_tick;
#undef THEADER
    return 0;
}

