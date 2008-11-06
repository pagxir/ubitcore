/* $Id$ */
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <queue>
#include <set>

#include "bthread.h"
#include "btimerd.h"

bthread::bthread()
{
    b_ident = "bthread";
    b_flag = BF_ACTIVE;
    b_runable = false;
    b_swaitident = NULL;
}

int
bthread::bdocall()
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
    b_runable = false;
    b_swaitident = ident;
    if (timeout > now_time()){
        benqueue(timeout);
    }else if (timeout > 0){
        b_runable = true;
        b_swaitident = NULL;
    }
}

static std::queue<bthread*> __q_running;

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
bthread::bpoll(bthread ** pu)
{

    if (__q_running.empty()){
        return -1;
    }
    _jnow = __q_running.front();
    __q_running.pop();
    _jnow->b_flag |= BF_ACTIVE;
    _jnow->b_runable = true;
    *pu = _jnow;
    return 0;
}

bthread*
bthread::now_job()
{
    return _jnow;
}

bthread *bthread::_jnow = NULL;
