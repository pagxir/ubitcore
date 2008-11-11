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
    b_index = index;
    b_start_time = now_time();
    b_ident = "refreshthread";
}

int
refreshthread::bdocall()
{
    int u, i, j;
    uint8_t mask;
    char tmpid[20];
    char bootid[20];
    size_t count;
    kitem_t items[8];
    int state = b_state;
    while (b_runable){
        b_state = state++;
        switch(b_state)
        {
            case 0:
                genkadid(bootid);
                getkadid(tmpid);
                j = 0;
                u = b_index;
                while (u >= 8){
                    bootid[j] = tmpid[j];
                    u -= 8;
                    j++;
                }
                mask =  __mask[u];
                bootid[j] = (tmpid[j]&mask)|(bootid[j]&~mask);
                if (get_table_size() > b_index){
                    bootid[j] ^= (0x80>>u);
                }
                count = find_nodes(bootid, items, true);
                b_find = kfind_new(bootid, items, count);
                break;
            case 1:
                if (b_find->vcall() == -1){
                    state = 2;
                }
                break;
            case 2:
                tsleep(NULL);
                b_state = 0;
                break;
            default:
                assert(0);
                break;
        }
    }
}
