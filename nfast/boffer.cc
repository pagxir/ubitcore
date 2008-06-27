/* $Id:$ */
#include <time.h>
#include "bthread.h"
#include "boffer.h"

class backport: public bthread
{
    public:
        backport();
        virtual int bdocall(time_t timeout);
};

static backport __backport;

int
boffer_start(int port)
{
    __backport.bwakeup();
    return 0;
}

backport::backport()
{
    b_ident = "backport";
}

int
backport::bdocall(time_t timeout)
{
    printf("hello world!\n");
    return 0;
}
