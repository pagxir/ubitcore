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

#define K0 "http://ftp.cdut.edu.cn/pub/slackware/10.1/slackware-10.1-install-d1.iso"
#define K3 "http://www.cfan.com.cn/school/adfadsfzcxvxb4sf2007-23.rar"
#define K1 "http://debian.nctu.edu.tw/ubuntu-releases/8.04/ubuntu-8.04-desktop-i386.iso"
#define K2 "http://www.cfan.com.cn/school/nmgjha2006-12.rar"

class burlthread: public bthread
{
    public:
        burlthread(const char *url, int second);
        virtual int bdocall(time_t timeout);
        ~burlthread();

    private:
        int b_state;
        int b_second;
        int last_time;
        char *b_url;
        burlget *b_get;
};

burlthread::burlthread(const char *url, int second)
{
    b_url = strdup(url);
    b_get = NULL;
    b_state = 0;
    b_second = second;
    last_time = time(NULL);
}

int
burlthread::bdocall(time_t timeout)
{
    int error = 0;
    int state = b_state;
    while(error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                b_get = burlget::get();
                assert(b_get != NULL);
                error = b_get->burlbind(b_url);
                break;
            case 1:
                error = b_get->bdopoll(timeout);
                break;
            case 2:
                assert(error==0);
                error = btime_wait(last_time+b_second);
                break;
            case 3:
                last_time = time(NULL);
                delete b_get;
                b_get = NULL;
                state = 0;
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
    if (b_url != NULL){
        free(b_url);
        b_url = NULL;
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
#if 0
    fprintf(stderr, "\rbcall(%s): %s", ident_text, ctime(&now));
#endif
    while(-1 != btime_wait(last_time+b_second)){
        last_time = time(NULL);
    }
    return -1;
}

int
main(int argc, char *argv[])
{
    burlthread urla(K0, 10), urlb(K1, 20), urlc(K2, 30), urld(K3, 40);
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
