/* $Id$ */
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <queue>

#include "bthread.h"
#include "btimerd.h"
#include "bidle.h"

class bidlethread: public bthread
{
    public:
        bidlethread();
        virtual int bdocall();
};

static bidlethread __idlethread;

bidlethread::bidlethread()
{
    b_ident = "bidlethread";
}

int
bidlethread::bdocall()
{
    sleep(1);
    printf("idle call\n");
    btimercheck();
    return 0;
}

int
get_idle(bthread **pu)
{
    *pu = &__idlethread;
    return 0;
}
