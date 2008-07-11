/* $Id$ */
#include <time.h>
#include "bthread.h"
#include "boffer.h"

class backport: public bthread
{
    public:
        backport();
        virtual int bdocall(time_t timeout);

    private:
        int b_state;
};

static backport __backport;

int
boffer_start(int port)
{
    __backport.bwakeup();
    return 0;
}

backport::backport():
    b_state(0)
{
    b_ident = "backport";
}

int
backport::bdocall(time_t timeout)
{
    int error = 0;
    int state = b_state;

    while (error != -1){
        b_state = state++;
        switch(b_state)
        {
            case 0:
                break;
            case 1:
                break;
            case 2:
                break;
            default:
                error = -1;
                break;
        }
    }
    return error;
}
