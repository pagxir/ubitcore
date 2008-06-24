#ifndef __BCLOCK_H__
#define __BCLOCK_H__
#include "bthread.h"

class bclock: public bthread
{
    public:
        bclock(const char *text, int second);
        virtual int bdocall(time_t timeout);
        ~bclock();

    private:
        int b_second;
        int last_time;
        std::string ident_text;
};
#endif
