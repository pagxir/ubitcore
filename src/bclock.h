#ifndef __BCLOCK_H__
#define __BCLOCK_H__
#include "bthread.h"

class bclock: public bthread
{
    public:
        bclock(const char *text, int second);
        virtual int bdocall();

    private:
        int b_second;
        time_t last_time;
        std::string ident_text;
};
#endif
