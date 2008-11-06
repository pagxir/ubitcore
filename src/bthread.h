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
    int flag(){ return b_flag; }
    static bthread *now_job();
    static int bpoll(bthread **pb);
    virtual int bfailed();
    virtual int bdocall();

public:
    int b_seed;
    int  b_flag;
    time_t b_tick;
    void   tsleep(void *ident);

private:
    void   *b_swaitident;
    static bthread *_jnow;

protected:
    bool b_runable;
    std::string b_ident;
};

#endif
