/* $Id$ */
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <queue>

#include "bthread.h"
#include "biothread.h"
#include "bsocket.h"

class biothread: public bthread
{
    public:
        biothread();
        virtual int bdocall();
};

static biothread __iothread;
static void *__iowait = &__iothread;

biothread::biothread()
{
    b_ident = "biothread";
    bsocket::global_init();
    b_swaitident = this;
}

int
biothread::bdocall()
{
    __iowait = NULL;
    tsleep(this);
    bsocket::bselect(0);
    __iowait = this;
    return 0;
}

int
biorun()
{
    __iothread.bwakeup(__iowait);
    return 0;
}
