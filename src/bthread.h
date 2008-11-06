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
    int btime_wait(time_t t);
    int benqueue(time_t timeout);
    int flag(){ return b_flag; }
    static time_t now_time();
    static bthread *now_job();
    static time_t check_timer();
    static int bpoll(bthread **pb, time_t *time);
    virtual int bfailed();
    virtual int bdocall(time_t  timeout);

public:
    int b_seed;
    time_t b_tick;
    void   tsleep(void *ident, time_t timeout);

private:
    void   *b_swaitident;
    static time_t _tnow;
    static bthread *_jnow;

protected:
    int  b_flag;
    bool b_runable;
    std::string b_ident;
};

#endif
