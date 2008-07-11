#ifndef __BFILED_H__
#define __BFILED_H__
#include "bthread.h"

class bfiled: public bthread
{
    public:
        bfiled(int second);
        virtual int bdocall(time_t timeout);

    private:
        int b_second;
        int last_time;
};

int badd_per_file(int piece, int start);
int bfiled_start(int time);
#endif
