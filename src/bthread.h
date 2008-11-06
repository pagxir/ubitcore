#ifndef __BTHREAD_H__
#define __BTHREAD_H__
#include <string>

#define BF_SELECT 0x1000
#define BF_ACTIVE 0x2000

class bthread
{
public:
    bthread();
    int bwakeup(void *ident);
    static bthread *now_job();
    static int bpoll(bthread **pb);
    virtual int bfailed();
    virtual int bdocall();

public:
    time_t b_tick;
    void tsleep(void *ident);

protected:
    int    b_pollable;
    void  *b_swaitident;
    static bthread *_jnow;
    static int _b_count;

protected:
    bool b_runable;
    std::string b_ident;
};

#endif
