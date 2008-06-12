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
        virtual int bdocall(time_t timeout);
};


biothread::biothread()
{
    bsocket::global_init();
}

int biothread::bdocall(time_t timeout)
{
    bsocket::bselect(timeout);
    return 0;
}

static biothread __iothread;

int biorun()
{
    __iothread.bwakeup();
    return 0;
}
