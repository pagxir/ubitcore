#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <algorithm>

#include "btkad.h"
#include "knode.h"
#include "kbucket.h"

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
kbucket::get_knode(kitem_t nodes[])
{
    int i;
    int count = std::min(b_count, _K);
    for (i=0; i<count; i++){
        b_knodes[i]->getnode(&nodes[i]);
    }
    return b_count;
}

int
kbucket::update_contact(const kitem_t *in, kitem_t *out)
{
    int i;
    int result = 0;
    knode *kn;
    kitem_t tout;
    time_t  now = time(NULL);
    time_t  lru = now;
    for (i=0; i<b_count; i++){
        kn = b_knodes[i];
        if (0==kn->replace(in, &tout)){
            return 0;
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
    return (now-lru>15*60);
}

static kbucket *__static_routing_table[160] = {NULL};
static int __boot_count = 0;
static kitem_t __boot_contacts[8];


int
get_knode(char target[20], kitem_t nodes[], bool valid)
{
    int count = 0;
    int i = get_kbucket_index(target);
    assert(i >= 0);
    kbucket *bucket = __static_routing_table[i];
    if (bucket != NULL){
        count = bucket->get_knode(nodes);
    }
    if (count == 0){
        /* route table is empty */
        int max = std::min(_K, __boot_count);
        for (i=0; i<max; i++){
            nodes[i] = __boot_contacts[i];
            genkadid(nodes[i].kadid);
        }
        count = max;
    }
    return count;
}

int
update_contact(const kitem_t *in, kitem_t *out)
{
    int i;
    const char *kadid = in->kadid;
    for (i=0; i<20; i++){
        printf("%02x", kadid[i]&0xFF);
    }
    printf("\n");
    i = get_kbucket_index(in->kadid);
    assert(i >= 0);
    kbucket *bucket = __static_routing_table[i];
    if (bucket == NULL){
        bucket = new kbucket;
        __static_routing_table[i] = bucket;
    }
    return bucket->update_contact(in, out);
}

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
