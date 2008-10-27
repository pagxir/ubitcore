#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <queue>
#include <stdint.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string>
#include <map>

#include "btcodec.h"
#include "butils.h"
#include "bsocket.h"
#include "bthread.h"
#include "bdhtident.h"
#include "bdhtnet.h"
#include "bdhtboot.h"
#include "bdhtorrent.h"

typedef struct  _rib{
    uint8_t ident[20];
    uint16_t port;
    uint32_t host;
}rib;


static int __ribcount = 0;
static bdhtboot *__bootlist[160];
static rib *__bucket[160][8];

int getribcount()
{
    return __ribcount;
}

static uint8_t __mask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

int
gentarget(char target[20], int which)
{
    /* which: which slot */
    int i;
    getclientid(target);
    for (i=0; i<20; i++){
        if (which < 8){
            break;
        }
        which -= 8;
    }
    target[i]=__mask[which]^target[i];
    for (i++;i<20;i++){
        target[i] = 0xFF&rand();
    }
    return 0;
}

void
update_route(bdhtnet *net, const void *ibuf, size_t len,
        uint32_t host, uint16_t port)
{
    size_t idl;
    btcodec codec;
    codec.bload((char*)ibuf, len);

    const char *ident = codec.bget().bget("r").bget("id").c_str(&idl);
    if (ident==NULL || idl!=20){
        return;
    }

    int i;
    char clientid[20];
    getclientid(clientid);
    bdhtident one((uint8_t*)ident), self((uint8_t*)clientid);
    bdhtident dist = one^self;
    int lg = dist.lg();
    for (i=0; i<8; i++){
        if (__bucket[lg][i] == NULL){
            __bucket[lg][i] = new rib;
            memcpy(__bucket[lg][i]->ident, ident, 20);
            __bucket[lg][i]->host = host;
            __bucket[lg][i]->port = port;
            __ribcount++;
            break;
        }else if (__bucket[lg][i]->host == host){
            return;
        }
    }
#if 1
    if (lg>0 && __bucket[lg-1][0]==NULL){
        char target[20];
        lg --;
        bdhtboot* &dhtboot = __bootlist[lg];
        if (dhtboot == NULL){
            dhtboot = new bdhtboot(net, lg);
            gentarget(target, lg);
            dhtboot->set_target((uint8_t*)target);
            printf("wakup: %02d:\t", lg);
            for (i=0; i<20; i++){
                printf("%02x", 0xff&target[i]);
            }
            printf("\n");
            dhtboot->add_dhtnode(host, port);
            dhtboot->bwakeup();
        }
    }
#endif
}

void
dump_route_table()
{
    int i, j;
    char clientid[20];
    printf("\nroute table begin\n");
    getclientid(clientid);
    printf("myid:\t");
    for (i=0; i<20; i++){
        printf("%02x", 0xff&(clientid[i]));
    }
    printf("\n---------------------------------\n");
    for (i=0; i<160; i++){
        for (j=0; j<8; j++){
            rib *vp = __bucket[i][j];
            if (vp != NULL){
                int u;
                printf("0x%02x:\t", i);
                for (u=0; u<20; u++){
                    printf("%02x", vp->ident[u]);
                }
                printf("\n");
            }
        }
    }
    printf("route table finish\n");
}

int
update_boot_nodes(int tabidx, uint32_t host[],
        uint16_t port[], uint8_t idents[][20], size_t size)
{
    int i;
    int count = 0;
    if (tabidx < 159){
        for (i=0; i<8; i++){
            if (__bucket[tabidx+1][i] == NULL){
                return count;
            }
            rib *rib = __bucket[tabidx+1][i];
            host[count] = rib->host;
            port[count] = rib->port;
            memcpy(idents[count], rib->ident, 20);
            count++;
            if (count >= size){
                return count;
            }
        }
    } 
    return count;
}

static bdhtorrent *__dhtorrent;

void
route_get_peers(bdhtnet *dhtnet)
{
    char clientid[20];
    __dhtorrent = new bdhtorrent(dhtnet);
    bdhtident info((uint8_t*)get_info_hash());
    getclientid(clientid);
    bdhtident ident((uint8_t*)clientid);

    bdhtident dist = info^ident;

    int i;
    int count = 0;
    int lg = dist.lg();
    for (; lg<160 && count<8; lg++){
        for (i=0; i<8&&count<8; i++){
            rib *r = __bucket[lg][i];
            if (r != NULL){
                count++;
                __dhtorrent->add_node(r->host,r->port);
            }
        }
    }
    for (lg=dist.lg()-1; lg>=0 && count<8; lg--){
        for (i=0; i<8&&count<8; i++){
            rib *r = __bucket[lg][i];
            if (r != NULL){
                count++;
                __dhtorrent->add_node(r->host,r->port);
            }
        }
    }
    __dhtorrent->set_infohash((uint8_t*)get_info_hash());
    __dhtorrent->bwakeup();
}

bool
route_need_update(int index)
{
    if (__bucket[index][7] == NULL){
        return true;
    }
    return false;
}
