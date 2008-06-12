#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "bthread.h"
#include "biothread.h"
#include "bhttpd.h"

class bclock: public bthread
{
    public:
        bclock(const char *text, int second);
        ~bclock();
        virtual int bdocall(time_t timeout);

    private:
        int second;
        int last_time;
        const char *ident_text;
};

bclock::bclock(const char *text, int second)
{
    this->second = second;
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
    printf("bcall(%s): %s", ident_text, ctime(&now));
    while(-1 != btime_wait(last_time+second)){
        last_time = time(NULL);
    }
    return -1;
}

int
main(int argc, char *argv[])
{
    bhttpd_start();
    bclock c("SYS", 4), d("DDD", 9), k("UFO", 9), e("XDD", 7), f("ODD", 3);
    c.bwakeup(); d.bwakeup(); k.bwakeup(); e.bwakeup(); f.bwakeup();
    bthread *j;
    time_t timeout;
    while (-1 != bthread::bpoll(&j, &timeout)){
        bwait_cancel();
        if (-1 == j->bdocall(timeout)){
            j->bwait();
        }
    }
    return 0;
}
