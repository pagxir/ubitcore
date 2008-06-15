#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "bthread.h"
#include "biothread.h"
#include "burlget.h"

#ifndef NDEBUG
#include "bsocket.h"
#endif

class burlthread: public bthread
{
    public:
        burlthread(int second);
        virtual int bdocall(time_t timeout);
        ~burlthread();

    private:
        int b_state;
        int b_second;
        int last_time;
        burlget *b_get;
};

burlthread::burlthread(int second)
{
    b_get = NULL;
    b_state = 0;
    b_second = second;
    last_time = time(NULL);
}

int burlthread::bdocall(time_t timeout)
{
    int error = 0;
    int state = b_state;
    while(error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                b_get = burlget::get();
                assert(b_get != NULL);
                break;
            case 1:
                error = b_get->bdopoll(timeout);
                break;
            case 2:
                if (error == 0){
                    delete b_get;
                    b_get = NULL;
                    state = 0;
                }
                break;
        }
    }
    return error;
}

burlthread::~burlthread()
{
    if (b_get != NULL){
        delete b_get;
        b_get = NULL;
    }
}

class bclock: public bthread
{
    public:
        bclock(const char *text, int second);
        virtual int bdocall(time_t timeout);
        ~bclock();

    private:
        int b_second;
        int last_time;
        const char *ident_text;
};

bclock::bclock(const char *text, int second)
{
    b_ident = "bclock";
    b_second = second;
    last_time = time(NULL);
    ident_text = strdup(text);
}

bclock::~bclock()
{
    free((void*)ident_text);
}

int
bclock::bdocall(time_t timeout)
{
    time_t now;
    time(&now);
    fprintf(stderr, "\rbcall(%s): %s", ident_text, ctime(&now));
    while(-1 != btime_wait(last_time+b_second)){
        last_time = time(NULL);
    }
    return -1;
}

int
main(int argc, char *argv[])
{
    burlthread urla(10), urlb(20), urlc(30), urld(40);
    urla.bwakeup();  urlb.bwakeup();  urlc.bwakeup(); urld.bwakeup();
    bclock c("SYS", 14), d("DDD", 19), k("UFO", 19), e("XDD", 17), f("ODD", 13);
    c.bwakeup(); d.bwakeup(); k.bwakeup(); e.bwakeup(); f.bwakeup();
    bthread *j;
    time_t timeout;
    while (-1 != bthread::bpoll(&j, &timeout)){
        bwait_cancel();
        if (-1 == j->bdocall(timeout)){
            j->bwait();
#ifndef NDEBUG
#endif
        }
    }
    return 0;
}
