#ifndef __BQUEUE_H__
#define __BQUEUE_H__
#include <memory>
#include "bthread.h"
#include "bpeermgr.h"

class bqueue: public bthread
{
    public:
        bqueue();
        virtual int bfailed();
        virtual int bdocall(time_t timeout);

    private:
        int b_state;
        ep_t *b_ep;
};
#endif
