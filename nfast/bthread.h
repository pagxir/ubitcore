#ifndef __UJOB_H__
#define __UJOB_H__

#define BF_SELECT 0x1000
#define BF_ACTIVE 0x2000

class bthread
{
public:
    bthread();
    int bwakeup();
    int bwait();
    int benqueue(time_t timeout);
    int flag(){ return b_flag; }
    static int bpoll(bthread **pb, time_t *time);
    virtual int bfailed();
    virtual int bdocall(time_t  timeout);

public:
    int b_seed;
    time_t b_tick;

protected:
    const char *b_ident;
    int b_flag;
};

typedef int (*j_caller)(bthread *, int , void *);
int bwait_cancel();
int btime_wait(int t);
int bthread_waiter(j_caller caller, int argc, void *argv);
#endif
