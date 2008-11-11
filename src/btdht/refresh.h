#ifndef __REFRESH_H__
#define __REFRESH_H__
#include "bthread.h"
class kfind;
class refreshthread: public bthread
{
    public:
        refreshthread(int index);
        virtual int bdocall();

    private:
        int b_index;
        int b_state;
        time_t b_random;
        time_t b_start_time;
        kfind  *b_find;
};
#endif
