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

static bool __iowait = false;
static biothread __iothread;

biothread::biothread()
{
    b_ident = "biothread";
    bsocket::global_init();
}

int
biothread::bdocall()
{
    __iowait = true;
    bsocket::bselect(0);
    __iowait = false;
    tsleep(NULL);
    return 0;
}

int
biorun()
{
    if (__iowait == false){
        __iothread.bwakeup(NULL);
    }
    return 0;
}
