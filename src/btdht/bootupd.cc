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
#include "bootupd.h"
#include "kfind.h"

bootupd::bootupd()
{
    b_find = 0;
    b_state = 0;
    b_ident = "bootupd";
    b_refresh = true;
    b_usevalid = false;
    b_start_time = now_time();
}

int
bootupd::bdocall()
{
    int count;
    int error = -1;
    int state = b_state;
    char bootid[20];
    kitem_t items[8];
    while (b_runable){
        b_state = state++;
        switch(b_state)
        {
            case 0:
                getkadid(bootid);
                bootid[19]^=0x1;
                count = find_nodes(bootid, items, b_usevalid);
                if (count == 0){
                    count = find_nodes(bootid, items, false);
                }
                if (count == 0){
                    count = get_bootup_nodes(items, 8);
                }
                if (count == 0){
                    tsleep(NULL, "exit");
                    return 0;
                }
                printf("booting .....: %d\n", count);
                b_find = kfind_new(bootid, items, count);
                break;
            case 1:
                b_random = (time_t)((60*15*0.9)+(rand()%(60*15))/5);
                b_start_time = now_time();
                break;
            case 2:
                error = b_find->vcall();
                break;
            case 3:
                printf("booting finish: %d\n", error);
                b_usevalid = (error<4);
                break;
            case 4:
                if (size_of_table() > 4){
                    if (b_refresh == true){
                        refresh_routing_table();
                        b_refresh = false;
                    }
                    btime_wait(b_start_time+b_random);
                }
                break;
            case 5:
                delete b_find;
                state = 0;
                break;
            default:
                assert(0);
                break;
        }
    }
}
