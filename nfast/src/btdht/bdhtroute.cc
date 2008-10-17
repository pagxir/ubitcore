#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <queue>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <map>

#include "buinet.h"
#include "btcodec.h"
#include "butils.h"
#include "bsocket.h"
#include "bthread.h"
#include "bdhtident.h"
#include "bdhtnet.h"
#include "bdhtorrent.h"

typedef struct  _rib{
    uint8_t ident[20];
    uint16_t port;
    uint32_t host;
}rib;


static int __ribcount = 0;
static rib *__bucket[160][8];

int getribcount()
{
    return __ribcount;
}

void
update_route(const void *ibuf, size_t len, uint32_t host, uint16_t port)
{
    size_t idl;
    btcodec codec;
    codec.bload((char*)ibuf, len);

    const char *ident = codec.bget().bget("r").bget("id").c_str(&idl);
    if (ident==NULL || idl!=20){
        return;
    }

    int i;
    bdhtident one((uint8_t*)ident), self((uint8_t*)get_peer_ident());
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
        }
    }
}

void
dump_route_table()
{
    int i, j;
    printf("\nroute table begin\n");
    printf("myid:\t");
    for (i=0; i<20; i++){
        printf("%02x", 0xff&(get_peer_ident()[i]));
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

static bdhtorrent *__dhtorrent;

void
route_get_peers(bdhtnet *dhtnet)
{
    __dhtorrent = new bdhtorrent(dhtnet);
    bdhtident info((uint8_t*)get_info_hash());
    bdhtident ident((uint8_t*)get_peer_ident());

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
