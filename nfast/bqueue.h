#ifndef __BQUEUE_H__
#define __BQUEUE_H__
#include "bthread.h"
class bqueue: public bthread
{
    public:
        virtual int bdocall(time_t timeout);
};
#endif
