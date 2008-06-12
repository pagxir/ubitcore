#include <stdio.h>
#include <time.h>

#include "bthread.h"
#include "bsocket.h"
#include "bhttpd.h"

class bhttpd: public bthread
{
    public:
        bhttpd();
        virtual int bdocall(time_t timeout);

    private:
        bsocket b_listener;
        int b_state;
};

bhttpd::bhttpd()
{
    b_state = 0;
}

int
bhttpd::bdocall(time_t timeout)
{
    int error = 0;
    int state = b_state;
    while(error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                error = b_listener.bconnect();
                break;
            case 1:
                error = b_listener.bsend();
                break;
            default:
                error = -1;
                break;
        }
    }
    return error;
}

static bhttpd __httpd;

int
bhttpd_start()
{
    __httpd.bwakeup();
    return 0;
}
