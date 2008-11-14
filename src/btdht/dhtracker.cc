#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <netinet/in.h>
#include <map>

#include "btkad.h"
#include "dhtracker.h"
#include "kfind.h"
#include "knode.h"
#include "kgetpeers.h"
#include "btimerd.h"


static uint8_t __mask[] = {0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF};

dhtrackerd::dhtrackerd(const char target[])
{
    b_state = 0;
    b_retry = 0;
    b_getpeers = NULL;
    b_ident = "dhtrackerd";
    b_usevalid = false;
    memcpy(b_info_hash, target, 20);
    b_last_update = now_time();
}

int
dhtrackerd::bdocall()
{
    char bootid[20];
    size_t count;
    kitem_t items[8];
    int error = 0;
    int state = b_state;
    while (b_runable){
        b_state = state++;
        switch(b_state)
        {
            case 0:
                count = find_nodes(b_info_hash, items, b_usevalid);
                printf("get peers: %s:%d\n",
                         idstr(b_info_hash), count);
                if (count == 0){
                    count = find_nodes(b_info_hash, items, false);
                }
                if (count == 0){
                    tsleep(NULL, "exit");
                    return 0;
                }
                b_getpeers = kgetpeers_new(bootid, items, count);
                b_last_update = time(NULL);
                break;
            case 1:
                error = b_getpeers->vcall();
                break;
            case 2:
                b_usevalid = (error<4);
                break;
            case 3:
                if (error<5 && b_retry<3){
                    b_retry++;
                }else if (b_last_update + 800 > time(NULL)){
                    tsleep(NULL, "select");
                }
                state = 0;
                break;
            default:
                assert(0);
                break;
        }
    }
    return error;
}
