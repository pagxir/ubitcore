#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <map>
#include <vector>
#include <algorithm>

#include "btkad.h"
#include "knode.h"
#include "kbucket.h"
#include "btimerd.h"
#include "kfind.h"
#include "bthread.h"

class refreshthread: public bthread
{
    public:
        refreshthread(int index);
        virtual int bdocall();

    private:
        int b_index;
        int b_state;
        time_t b_random;
        time_t b_start_time;
        kfind  *b_find;
};

kbucket::kbucket()
    :b_count(0), b_last_seen(0)
{
}

void
kbucket::touch()
{
    b_last_seen = time(NULL);
}

int
kbucket::failed_contact(const kitem_t *node)
{
    int i;
    for (i=0; i<b_count; i++){
        b_knodes[i]->failed_contact(node);
    }
    return 0;
}

int
kbucket::find_nodes(kitem_t nodes[])
{
    int i;
    int count = 0;
    assert(b_count < _K);
    for (i=0; i<b_count; i++){
        if (b_knodes[i]->validate()){
            b_knodes[i]->getnode(&nodes[count]);
            count++;
        }
    }
    return count;
}

int
kbucket::get_knode(kitem_t nodes[])
{
    int i;
    int count = 0;
    assert(b_count < _K);
    for (i=0; i<b_count; i++){
        if (b_knodes[i]->validate()){
            b_knodes[i]->getnode(&nodes[count]);
            count++;
        }
    }
    return count;
}

int
kbucket::update_contact(const kitem_t *in, kitem_t *out)
{
    int i;
    int result = 0;
    knode *kn;
    knode *kkn = NULL;
    kitem_t tout;
    time_t  now = time(NULL);
    time_t  lru = now;
    for (i=0; i<b_count; i++){
        kn = b_knodes[i];
        if (0==kn->replace(in, &tout)){
            //printf("replace it\n");
            return 0;
        }
        if (kn->validate() == false){
            kkn = kn;
        }
        if (tout.atime < lru){
            lru = tout.atime;
            *out = tout;
        }
    }
    if (b_count < _K){
        kn  = new knode(in->kadid, in->host, in->port);
        kn->touch();
        b_knodes[b_count++] = kn;
        //printf("adding one knode\n");
        return 0;
    }
    if (kkn!=NULL){
        kkn->set(in);
        return 0;
    }
    //printf("drop: %d\n", b_count);
    return (now-lru>15*60);
}

static int __boot_count = 0;
static kitem_t __boot_contacts[8];
static refreshthread *__static_refresh[160] = {NULL};

int
update_boot_contact(in_addr_t addr, in_port_t port)
{
    int idx = __boot_count++;
    if (__boot_count > 8){
        idx = (rand()&0x7);
    }
    __boot_contacts[idx].host = addr;
    __boot_contacts[idx].port = port;
    return 0;
}

void dump_kbucket(kbucket *bucket)
{
    int i;
    kitem_t nodes[_K];
    int count = bucket->get_knode(nodes);
    for (i=0; i<count; i++){
        int j;
        printf("\t%02x: ", i);
        printf("%s:%d ", inet_ntoa(*(in_addr*)&nodes[i].host),
                htons(nodes[i].port));
        for (j=0; j<20; j++){
            printf("%02x", 0xff&nodes[i].kadid[j]);
        }
        printf("#%s", nodes[i].failed?"validate":"invalidate");
        printf("\n");
    }
}

void
dump_routing_table()
{
}

static uint8_t __mask[] = {0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF};

refreshthread::refreshthread(int index)
{
    b_find = NULL;
    b_state = 0;
    b_index = index;
    b_swaitident = __mask;
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
                bootid[j] ^= (0x80>>u);
                assert(b_index == get_kbucket_index(bootid));
                b_find = btkad::find_node(bootid);
                break;
            case 1:
                if (b_find->vcall() == -1){
                    state = 2;
                }
                break;
            case 2:
                tsleep(&__mask);
                b_state = 0;
                break;
            default:
                assert(0);
                break;
        }
    }
}

int refresh_routing_table()
{
    int i;
#if 0
    for (i=0; i<__rfirst; i++){
        assert(__static_refresh[i]);
        __static_refresh[i]->bwakeup(__mask);
    }
#endif
    return 0;
}
