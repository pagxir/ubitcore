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
bthread::tsleep(void *ident)
{
    assert(b_runable==true);
    b_runable = false;
    b_swaitident = ident;
}

static std::queue<bthread*> __q_running;

int
bthread::bwakeup(void *wait)
{
    if (b_runable == true){
        return 0;
    }
    assert(b_swaitident==wait);
    __q_running.push(this);
    b_runable = true;
    return 0;
}

int
bthread::bpoll(bthread ** pu)
{
    do {
        if (__q_running.empty()){
            return -1;
        }
        _jnow = __q_running.front();
        __q_running.pop();
    }while(!_jnow->b_runable);
    __q_running.push(_jnow);
    *pu = _jnow;
    return 0;
}

bthread*
bthread::now_job()
{
    return _jnow;
}

bthread *bthread::_jnow = NULL;
