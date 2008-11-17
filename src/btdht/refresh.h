#ifndef __REFRESH_H__
#define __REFRESH_H__
#include "kutils.h"
#include "bthread.h"
class kfind;
class refreshd: public bthread
{
    public:
        refreshd(int index);
        time_t last_update(){ return b_last_update; }
        virtual int bdocall();

    private:
        int b_index;
        int b_state;
        kitem_t b_findNodes[8];

    private:
        time_t b_random;
        time_t b_last_update;
        kfind  *b_find;

    private:
        int b_retry;
        bool b_usevalid;
};
#endif
