/* $Id$ */
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <queue>
#include <set>

#include "bthread.h"

struct bresume{
    int br_argc;
    void *br_argv;
    j_caller br_caller;
};

std::queue<bresume> __q_bresume;

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
    if (__q_bresume.empty()) {
        return bfailed();
    }
#if 0
    if (__q_bresume.size() > 2){
        printf("dupit wakeup!\n");
    }
#endif
    while (!__q_bresume.empty()){
        bresume bt = __q_bresume.front();
        __q_bresume.pop();
        (*bt.br_caller)(this,
                bt.br_argc,
                bt.br_argv);
    }
    return 0;
}

int
bwait_cancel()
{
    while(!__q_bresume.empty()){
        __q_bresume.pop();
    }
    return 0;
}

int
bthread_waiter(j_caller caller, int argc, void *argv)
{
    bresume resume;
    resume.br_argc = argc;
    resume.br_argv = argv;
    resume.br_caller = caller;
    __q_bresume.push(resume);
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

