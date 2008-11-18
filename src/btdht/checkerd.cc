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

static int count;
static char bootid[20];
static kitem_t items[8];
static int __index = 0;
static kfind *__find = NULL;
static time_t __wait = 0;
static bool   __retry = false;

int
checkerd::bdocall()
{
    int error;
    int state = b_state;
    while (b_runable){
        b_state = state++;
        switch(b_state)
        {
            case 0:
                if (__index >= size_of_table()){
                    tsleep(NULL, "exit");
                    return 0;
                }
                genrefreshid(bootid, __index);
                count = find_nodes(bootid, items, __retry);
                if (count == 0){
                    tsleep(NULL, "exit");
                    return 0;
                }
                __find = kfind_new(bootid, items, count);
                __retry = false;
                __index++;
                break;
            case 1:
                error = __find->vcall();
                break;
            case 2:
                if (error == 0){
                    printf("refresh return zero:\n");
                    __retry = true;
                    __index--;
                }
                delete __find;
                if (__index < size_of_table()){
                    state = 0;
                }
                break;
            case 3:
                __wait = time(NULL)+800;
                break;
            case 4:
                dump_routing_table();
                if (time(NULL)>__wait+800){
                    btime_wait(__wait+800);
                }
                break;
            case 5:
                __index = 0;
                state = 0;
                break;
#if 0
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
#endif
            default:
                assert(0);
                break;
        }
    }
    return 0;
}
