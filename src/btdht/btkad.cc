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
#include "kfind.h"
#include "bsocket.h"
#include "btcodec.h"
#include "transfer.h"
#include "refresh.h"
#include "kgetpeers.h"
#include "dhtracker.h"
#include "provider.h"
#include "checkerd.h"
#include "btimerd.h"
#include "bootupd.h"
#include "kpingd.h"

static int __boot_count = 0;
static kitem_t __boot_contacts[8];
static refreshd *__static_refresh[160];

static pingd __static_pingd;
static ktable __static_table;
static bdhtnet __static_dhtnet;
static bootupd __static_bootupd;
static checkerd __static_checkerd;

kship *
kship_new()
{
    return __static_dhtnet.get_kship();
}

kfind *
kfind_new(char target[20], kitem_t items[], size_t count)
{
    return new kfind(&__static_dhtnet, target, items, count);
}

kgetpeers *
kgetpeers_new(char target[20], kitem_t items[], size_t count)
{
    return new kgetpeers(&__static_dhtnet, target, items, count);
}

int
getkadid(char kadid[20])
{
    __static_table.getkadid(kadid);
    return 0;
}

int
genkadid(char kadid[20])
{
    int i;
    /* on windows, this rand() is limited to 65535 */
    uint16_t *vp = (uint16_t*)kadid;
    for (i=0; i<10; i++){
        vp[i] = rand();
    }
    return 0;
}

int
setkadid(const char kadid[20])
{
    printf("setkadid: %s\n", idstr(kadid));
    __static_table.setkadid(kadid);
    return 0;
}

int
update_contact(const kitem_t *in, bool contacted)
{
    static int __callonce = 0;
    if (__callonce++ == 0){
        printf("update contact\n");
    }
    int retval =__static_table.insert_node(in, contacted);
    if (__static_table.pingable()){
        __static_pingd.bwakeup(NULL);
    }
    return retval;
}

int
failed_contact(const kitem_t *in)
{
    return __static_table.failed_contact(in);
}

bool
table_is_pingable()
{
    return __static_table.pingable();
}

int
get_table_ping(kitem_t items[], size_t size)
{
    return __static_table.get_ping(items, size);
}


int
add_boot_contact(in_addr_t addr, in_port_t port)
{
    if (__boot_count < 8){
        int idx = __boot_count++;
        __boot_contacts[idx].host = addr;
        __boot_contacts[idx].port = port;
        return 0;
    }
    return 0;
}

int
get_bootup_nodes(kitem_t items[], size_t size)
{
    int count = __boot_count>size?size:__boot_count;
    memcpy(items, __boot_contacts, sizeof(kitem_t)*count);
    return count;
}

int
find_nodes(const char *target, kitem_t items[_K], bool valid)
{
    return __static_table.find_nodes(target, items, valid);
}

int
refresh_routing_table()
{
    int i;
    printf("refresh routing\n");
    for (i=0; i<__static_table.size(); i++){
        if (__static_refresh[i] == NULL){
            __static_refresh[i] = new refreshd(i);
        }
        __static_refresh[i]->bwakeup(NULL);
    }
    return 0;
}

void
dump_routing_table()
{

#if 1
    printf("refresh: ");
    time_t now = time(NULL);
    for (int i=0; i<__static_table.size(); i++){
        if (__static_refresh[i] != NULL){
            printf("%4d ", now-__static_refresh[i]->last_update());
        }
    }
    printf("\n");
    __static_table.dump();
#endif
}


int
bdhtnet_start()
{
    static int __call_once_only = 0;

    if (__call_once_only++ > 0){
        return -1;
    }
    char bootid[20];
    genkadid(bootid);
    setkadid(bootid);
    extern char __ping_node[];
    getkadid(&__ping_node[12]);
    extern char __find_node[];
    getkadid(&__find_node[12]);
    extern char __get_peers[];
    getkadid(&__get_peers[12]);
    __static_bootupd.bwakeup(NULL);
    __static_checkerd.bwakeup(NULL);
    return 0;
}

int
size_of_table()
{
    return __static_table.size();
}

int
size_of_bucket(int index)
{
    return __static_table.size_of_bucket(index);
}

int
bit1_index_of(const char kadid[20])
{
    return __static_table.bit1_index_of(kadid);
}

int
tracker_start(const char info_hash[20])
{
    dhtrackerd *d = new dhtrackerd(info_hash);
    d->bwakeup(NULL);
    return 0;
}
