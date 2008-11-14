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

refreshthread::refreshthread(int index)
{
    b_find = NULL;
    b_state = 0;
    b_retry = 0;
    b_index = index;
    b_need_validate = false;
    b_start_time = now_time();
    b_ident = "refreshthread";
}

int genbootid(char bootid[20], int index)
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
    if (get_table_size() > index){
        bootid[j] ^= (0x80>>u);
        assert(bit1_index_of(bootid) == index);
    }
    return 0;
}

int
refreshthread::bdocall()
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
                genbootid(bootid, b_index);
                count = find_nodes(bootid, items, b_need_validate);
                printf("refresh: %02d:%s:%d\n", b_index, idstr(bootid), count);
                if (count == 0){
                    count = find_nodes(bootid, items, false);
                }
                if (count == 0){
                    tsleep(NULL, "exit");
                    return 0;
                }
                b_find = kfind_new(bootid, items, count);
                b_start_time = time(NULL);
                break;
            case 1:
                error = b_find->vcall();
                if (error == -1){
                    state = 2;
                }
                break;
            case 2:
                b_need_validate = (error>4)?false:true;
                break;
            case 3:
                if (size_of_bucket(b_index)<5 && b_retry<3){
                    b_retry++;
                }else if (b_start_time+800 > time(NULL)){
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
