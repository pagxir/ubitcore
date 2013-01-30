#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h>
#include <map>

#include "btkad.h"
#include "refresh.h"
#include "kfind.h"
#include "knode.h"
#include "btimerd.h"


static uint8_t __mask[] = {0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF};

refreshd::refreshd(int index)
{
    b_find = NULL;
    b_state = 0;
    b_retry = 0;
    b_index = index;
    b_ident = "refreshd";
    b_usevalid = false;
    b_last_update = now_time();
}

int genrefreshid(char bootid[20], int index)
{
    uint8_t mask;
    char tmpid[20];
    int u, i, j;
    genkadid(bootid);
    getkadid(tmpid);
    j = 0;
    u = index;
    while (u >= 8){
        bootid[j] = tmpid[j];
        u -= 8;
        j++;
    }
    mask =  __mask[u];
    bootid[j] = (tmpid[j]&mask)|(bootid[j]&~mask);
    if (size_of_table() > index){
        bootid[j] ^= (0x80>>u);
        assert(bit1_index_of(bootid) == index);
    }
    return 0;
}

int
refreshd::bdocall()
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
                genrefreshid(bootid, b_index);
                count = find_nodes(bootid, items, b_usevalid);
                printf("refresh: %02d:%s:%d\n", b_index, idstr(bootid), count);
                if (count == 0){
                    count = find_nodes(bootid, items, false);
                }
                if (count == 0){
                    tsleep(NULL, "exit");
                    return 0;
                }
                b_find = kfind_new(bootid, items, count);
                b_last_update = time(NULL);
                break;
            case 1:
                error = b_find->vcall();
                break;
            case 2:
                if (error < 4 && b_retry<3){
                    b_usevalid = true;
                    b_retry++;
                    state = 0;
                }
                break;
            case 3:
                if (b_last_update + 800 > time(NULL)){
                    b_retry = 0;
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
