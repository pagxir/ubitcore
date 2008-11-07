/* $Id$ */
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <queue>

#include "bthread.h"
#include "btimerd.h"
#include "bsocket.h"
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
    time_t now = now_time();
    if (_b_count > 0){
        _b_count = 0;
        return 0;
    }
    time_t sel = comming_time();
    int twait = sel-now;
    assert(twait >= 0);
    bsocket::bselect(twait);
    return 0;
}

int
bidlerun()
{
    __idlethread.bwakeup(NULL);
    return 0;
}
