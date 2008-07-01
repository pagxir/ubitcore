/* $Id$ */
#include <stdio.h>

#include "bupdown.h"

bupdown::bupdown()
    :b_state(0), b_ep(NULL)
{
    b_ident = "bupdown";
}

int
bupdown::bfailed()
{
    return bthread::bfailed();
}

int
bupdown::bdocall(time_t timeout)
{
    int error = 0;
    int state = b_state;
    while (error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                if (b_ep != NULL){
                    delete b_ep;
                    b_ep = NULL;
                }
                error = bready_pop(&b_ep);
                break;
            case 1:
                printf("Hello World!\n");
                break;
            default:
                state = 0;
                break;
        }
    }

    return error;
}
