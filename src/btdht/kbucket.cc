#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <vector>
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
            printf("replace it\n");
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
        printf("adding one knode\n");
        return 0;
    }
    printf("drop: %d\n", b_count);
    return (now-lru>15*60);
}

static int __rfirst = 0;
static int __rsecond = 0;
static int __rself_count = 0;
static int __boot_count = 0;
static kbucket *__static_routing_table[160] = {NULL};
static kitem_t __boot_contacts[8];

struct ktarget_op{
    ktarget_op(const char *target);
    bool operator () (const kitem_t &l, const kitem_t &r);

    const char *b_target;
};

ktarget_op::ktarget_op(const char *target)
    :b_target(target)
{
}

bool ktarget_op::operator () (const kitem_t &l, const kitem_t &r)
{
    kaddist_t d1(l.kadid, b_target), d2(r.kadid, b_target);

    return d1 < d2;
}

int
kitem_trim(kitem_t nodes[], size_t size, char target[20], int vsize)
{
    std::vector<kitem_t> vect;
    std::partial_sort(vect.begin(), vect.begin()+vsize,
            vect.end(), ktarget_op(target));
    std::vector<kitem_t>::iterator iter=vect.begin();
    for (int i=0; i<vsize; i++){
        nodes[i] = *iter++;
    }
    return 0;
}

int
get_knode(char target[20], kitem_t nodes[], bool valid)
{
    kbucket *bucket;
    kitem_t  vnodes[_K];
    int count = 0;
    int vcall = 1;
    int vcount = 0;
    int i = get_kbucket_index(target);
    int min = i-1, max = i+1;
    assert(i >= 0);
    if (i < __rfirst){
        bucket = __static_routing_table[i];
        count = bucket->get_knode(nodes);
        while(count<8 && vcall>0){
            vcount = vcall = 0;
            if (min >= 0){
                bucket = __static_routing_table[min--];
                vcount += bucket->get_knode(&vnodes[vcount]);
                vcall++;
            }
            if (max < __rsecond){
                bucket = __static_routing_table[max++];
                vcount += bucket->get_knode(&vnodes[vcount]);
                vcall++;
            }
            if (vcount+count < 8){
                memcpy(&nodes[count], vnodes, sizeof(kitem_t)*vcount);
                count += vcount;
                continue;
            }
            kitem_trim(vnodes, vcount, target, 8-count);
            memcpy(&nodes[count], vnodes, sizeof(kitem_t)*(8-count));
            count = 8;
            break;
        }
        assert(count < 8);
        return count;
    }
    if (i < __rsecond){
        for (int j=__rfirst; j<__rsecond; j++){
            bucket = __static_routing_table[j];
            assert(bucket != NULL);
            count += bucket->get_knode(&nodes[count]);
        }
        printf("%d %d %d\n", __rfirst, __rsecond, count);
        assert(count < 8);
        return count;
    }
    /* route table is empty */
    count = std::min(_K, __boot_count);
    for (i=0; i<count; i++){
        nodes[i] = __boot_contacts[i];
        genkadid(nodes[i].kadid);
    }
    return count;
}

int
update_contact(const kitem_t *in, kitem_t *out)
{
    kbucket *bucket;
    const char *kadid = in->kadid;
#if 1
    printf("update_contact: ");
    for (int l=0; l<20; l++){
        printf("%02x", kadid[l]&0xFF);
    }
    printf("\n");
#endif
    int rsecond = get_kbucket_index(kadid);
    int index = rsecond++;
    if (index == 160){
        return 0;
    }
    for (;__rsecond<rsecond; __rsecond++){
        kbucket **table = __static_routing_table;
        table[__rsecond] = new kbucket;
    }
    __rself_count ++;
    for (; __rself_count>8; __rfirst++){
        kitem_t items[_K];
        int j = __rfirst;
        assert(j<__rsecond);
        bucket = __static_routing_table[j];
        __rself_count -= bucket->get_knode(items);
    }
    if (index < __rfirst){
        __rself_count--;
    }
    bucket = __static_routing_table[index];
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

void dump_kbucket(kbucket *bucket)
{
    int i;
    kitem_t nodes[_K];
    int count = bucket->get_knode(nodes);
    for (i=0; i<count; i++){
        int j;
        printf("\t%02x: ", i);
        for (j=0; j<20; j++){
            printf("%02x", 0xff&nodes[i].kadid[j]);
        }
        printf("\n");
    }
}

void
dump_routing_table()
{
    int i;
    char selfid[20];
    getkadid(selfid);
    printf("selfid: ");
    for (i=0; i<20; i++){
        printf("%02x", selfid[i]&0xff);
    }
    printf("\n");
    for(i=0; i<160; i++){
        if (__static_routing_table[i]){
            printf("kbucket: %02x\n", i);
            dump_kbucket(__static_routing_table[i]);
            printf("\n");
        }
    }
}
