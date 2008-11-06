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

static int __wait;
static biothread __iothread;

biothread::biothread()
{
    b_ident = "biothread";
    b_swaitident = &__wait;
    bsocket::global_init();
}

int
biothread::bdocall()
{
    bsocket::bselect(0);
    return 0;
}

int
biorun()
{
    __iothread.bwakeup(&__wait);
    return 0;
}
