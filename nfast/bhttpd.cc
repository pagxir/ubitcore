#include <stdio.h>
#include <time.h>

#include "bthread.h"
#include "bhttpd.h"

class bhttpd: public bthread
{
    public:
        virtual int bdocall(time_t timeout);
};

int
bhttpd::bdocall(time_t timeout)
{
    printf("Hello World!\n");
    return 0;
}

static bhttpd __httpd;

int
bhttpd_start()
{
    __httpd.bwakeup();
    return 0;
}
