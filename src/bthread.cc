/* $Id$ */
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <queue>
#include <set>

#include "bthread.h"
#include "btimerd.h"
#include "bidle.h"

bthread::bthread()
{
    memset(b_wmsg, 0x0, sizeof(b_wmsg));
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
bthread::tsleep(void *ident, const char wmsg[])
{
    if (b_runable == true){
        b_runable = false;
        b_swaitident = ident;
        strncpy(b_wmsg, wmsg, sizeof(b_wmsg));
    }
}

static std::queue<bthread*> __q_running;

int
bthread::bwakeup(void *wait)
{
    if (b_runable == true){
        return 0;
    }
    if (b_swaitident!=wait){
#if 0
        printf("not wakeupable: %p %p %s %s\n",
                b_swaitident, wait, b_ident.c_str(), b_wmsg);
#endif
        return 0;
    }
    _b_count++;
    __q_running.push(this);
    b_swaitident = NULL;
    b_runable = true;
    b_wmsg[0] = 0;
    return 0;
}

bool
bthread::reset_timeout()
{
    bool oldval = b_timeout;
    b_timeout  = false;
    return oldval;
}

int
bthread::timeout()
{
    b_timeout = true;
    bwakeup(b_swaitident);
}

int
bthread::bpoll(bthread ** pu)
{
    do {
        if (__q_running.empty()){
            assert(0);
            return 0;
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
int bthread::_b_count = 0;
