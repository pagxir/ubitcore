#ifndef __DHTRACKER_H__
#define __DHTRACKER_H__
#include "bthread.h"
class kgetpeers;
class dhtrackerd: public bthread
{
    public:
        dhtrackerd(const char target[20]);
        virtual int bdocall();

    private:
        char b_info_hash[20];
        int b_state;

    private:
        time_t b_random;
        time_t b_last_update;
        kgetpeers  *b_getpeers;

    private:
        int b_retry;
        bool b_usevalid;
};
#endif
