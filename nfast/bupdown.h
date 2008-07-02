#ifndef __BUPDOWN_H__
#define __BUPDOWN_H__
#include <memory>
#include "bthread.h"
#include "bpeermgr.h"
#include "bsocket.h"

class bupdown: public bthread
{
    public:
        bupdown();
        virtual int bfailed();
        virtual int bdocall(time_t timeout);

    private:
        ep_t *b_ep;
        int b_state;
        int b_offset;
        char b_buffer[1<<14];
};
#endif
