#ifndef __REFRESH_H__
#define __REFRESH_H__
#include "bthread.h"
class kfind;
class refreshd: public bthread
{
    public:
        refreshd(int index);
        virtual int bdocall();

    private:
        int b_index;
        int b_state;

    private:
        time_t b_random;
        time_t b_last_update;
        kfind  *b_find;

    private:
        int b_retry;
        bool b_usevalid;
};
#endif
