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
#include "ident.h"
#include "provider.h"
#include "boot.h"
#include "tracker.h"

typedef struct  _rib{
    uint8_t ident[20];
    uint16_t port;
    uint32_t host;
    time_t   addtime;
    int      ttl;
}rib;


static int __kmax = 0;
static int __ribcount = 0;
static bool __boot0 = true;
static bdhtboot *__bootlist[160];
static bdhtboot *__bootnextlist[160];
static rib *__bucket[160][16];

static std::map<bdhtident, rib*> __node_world;

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

#define new_rib new rib

    rib **replce_rib = NULL;
    rib *this_rib = NULL;
    if (__node_world.find(one) != __node_world.end()){
        this_rib = __node_world.find(one)->second;
    }else{
        this_rib = new_rib;
        __node_world.insert(std::make_pair(one, this_rib));
    }
    this_rib->host = host;
    this_rib->port = port;
    this_rib->ttl  = 5;
    this_rib->addtime = time(NULL);
    int lg = dist.lg();
    int good = 0;
    int keep = -1;
    int last_rib = 0;
    for (i=0; i<8; i++){
        rib * &bucket = __bucket[lg][i];
        if (bucket == NULL){
            /*add new node, end of this bucket */
            break;
        }else if (bucket == this_rib){
            /* update access time */
            good++;
            continue;
        }else if (bucket->ttl < 0){
            /* replace bad node */
            continue;
        }else if (bucket->addtime+60*15<time(NULL)){
            /* replace a doule node, the replace node will ping */
            /* put the new node here, and add old node to ping_queue *
             * when ping success the old node will put back again    */
            __bucket[lg][last_rib++] = bucket;
        }else if (last_rib < i){
            __bucket[lg][last_rib++] = bucket;
            good++;
        }
    }
    if (last_rib < 8){
        __bucket[lg][last_rib++] = this_rib;
        __bucket[lg][last_rib] = NULL;
        for (i=8; i<16; i++){
            if (__bucket[lg][i] == NULL){
                break;
            }
            __bucket[lg][i] = NULL;
        }
    }else for (i=8; i<16; i++){
        rib * &bucket = __bucket[lg][i];
        if (bucket == NULL){
            /* start ping */
            replce_rib = &bucket;
            break;
        }
        if (bucket == this_rib){
            replce_rib = &bucket;
            break;
        }
        if (bucket->ttl < 0){
            replce_rib = &bucket;
            /* start ping */
            break;
        }
        if (replce_rib == NULL){
            replce_rib = &bucket;
        }else if ((*replce_rib)->addtime > bucket->addtime){
            replce_rib = &bucket;
        }
    }
    if (replce_rib != NULL){
        *replce_rib = this_rib;
    }
    __kmax = std::max(__kmax, lg);
#if 1
    if (lg>0 && __bucket[lg-1][0]==NULL && __boot0==true){
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
    printf("\n------------------------------------------------------\n");
    for (i=0; i<160; i++){
        for (j=0; j<8; j++){
            rib *vp = __bucket[i][j];
            if (vp != NULL){
                int u;
                printf("0x%02x.%d:\t", i, j);
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
    char infohash[20];
    __dhtorrent = new bdhtorrent(dhtnet);
    get_info_hash(infohash);
    bdhtident info((uint8_t*)infohash);
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
    __dhtorrent->set_infohash((uint8_t*)infohash);
}

bool
route_need_update(int index)
{
    int i;
    for (i=7; i>=0; i--){
        rib * &bucket = __bucket[index][i];
        if (bucket == NULL){
            /* can add more node */
            return true;
        }
        if (bucket->ttl < 5){
            /* have bad node */
            return true;
        }
        if (bucket->addtime+60*15 < time(NULL)){
            /* have double node */
            return true;
        }
    }
    return false;
}

int
update_all_bucket(bdhtnet *net)
{
    int i, j;
    int count = 0;
    rib *lastrib[8]={NULL};
    if (__boot0 == false){
        return -1;
    }
    __boot0 = false;
    bool need_boot = false;
    for (i=__kmax; i>=0; i--){
        need_boot = false;
        for (j=0; j<8; j++){
            rib *current = __bucket[i][j];
            if (current != NULL){
                lastrib[count&0x7] = current;
                count++;
            }else{
                need_boot = true;
            }
        }
        if (need_boot==true&&count>8){
            char target[20];
            gentarget(target, i);
            bdhtboot* &dhtboot = __bootnextlist[i];
            dhtboot = new bdhtboot(net, i);
            dhtboot->set_target((uint8_t*)target);
#if 1
            for (j=0; j<8; j++){
                dhtboot->add_dhtnode(lastrib[j]->host,
                        lastrib[j]->port);
            }
#endif
        }
    }
    return 0;
}
