#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <map>

#include "btkad.h"
#include "knode.h"
#include "kbucket.h"
#include "ktable.h"
#include "bthread.h"
#include "bsocket.h"
#include "btcodec.h"
#include "transfer.h"
#include "provider.h"
#include "btimerd.h"
#include "checkerd.h"
#include "kfind.h"


checkerd::checkerd()
{
    b_state = 0;
    b_ident = "checkerd";
    b_last_show = now_time();
    b_next_refresh = now_time();
}

int
checkerd::bdocall()
{
    int state = b_state;
    while (b_runable){
        b_state = state++;
        switch(b_state)
        {
            case 0:
                btime_wait(b_last_show+120);
                if (now_time() > b_next_refresh){
                    time_t random = (time_t)((60*15*0.9)+(rand()%(60*15))/5);
                    b_next_refresh = now_time()+random;
                    refresh_routing_table();
                }
                break;
            case 1:
                dump_routing_table();
                b_last_show = now_time();
                state = 0;
                break;
            default:
                assert(0);
                break;
        }
    }
    return 0;
}
