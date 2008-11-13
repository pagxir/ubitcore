#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>

/*#include "kitem.h"*/
#include "btkad.h"
#include "knode.h"
#include "kbucket.h"
#include "ktable.h"
#include "kutils.h"

static int
bit1_start_at(const char *target)
{
    int i;
    int index = 0;
    uint8_t midx = 0x0;
    static const uint8_t mtable[] = {
        0x8, 0x7, 0x6, 0x6, 0x5, 0x5, 0x5, 0x5, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 
        0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 
        0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 
        0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 
        0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 
        0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 
        0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 
        0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 
    };
    for (i=0; i<20&&midx==0; i++){
        midx = (uint8_t)target[i];
        index += mtable[midx];
    }
#if 0
    printf("start index: %d\n", index);
#endif
    return index;
}

int
ktable::failed_contact(const kitem_t *in)
{
    const char *kadid = in->kadid;
    int index = bit1_index_of(kadid);
    if (index < b_nbucket1){
        b_buckets[index].failed_contact(in);
    }
    return 0;
}

int
ktable::bit1_index_of(const char *target)
{
    int i;
    char shadow[20];
    for (i=0; i<20; i++){
        shadow[i] = target[i]^b_tableid[i];
    }
    return bit1_start_at(shadow);
}

int
ktable::find_nodes(const char target[20], kitem_t items[8], bool validate)
{
    int i;
    int count = 0;
#if 0
    printf("find node: ");
#endif
    int offset = bit1_index_of(target);

    kitem_t vitems[8];
    int backoff = offset>b_nbucket1?b_nbucket1:offset;
    for (i=offset; i<b_nbucket1; i++){
        int n = b_buckets[i].find_nodes(vitems, validate);
        if (n+count >= 8){
            memcpy(&items[count], vitems, sizeof(kitem_t)*(8-count));
            count = 8;
            break;
        }
        memcpy(&items[count], vitems, sizeof(kitem_t)*n);
        count += n;
    }
    for (i=backoff-1; i>=0; i--){
        int n = b_buckets[i].find_nodes(vitems, validate);
        if (n+count >= 8){
            memcpy(&items[count], vitems, sizeof(kitem_t)*(8-count));
            count = 8;
            break;
        }
        memcpy(&items[count], vitems, sizeof(kitem_t)*n);
        count += n;
    }
    return count;
}

int
ktable::insert_node(const kitem_t *in, bool contacted)
{
    kitem_t items[_K];
    const char *kadid = in->kadid;
    int nbucket1 = bit1_index_of(kadid);
    int index = nbucket1++;

    if (nbucket1 > 160){
        printf("internal error: %s %s %s:%d!\n", idstr(kadid), idstr(b_tableid),
                inet_ntoa(*(in_addr*)&in->host), htons(in->port));
        return 0;
    }

    if (nbucket1 > b_nbucket1){
        b_nbucket1 = nbucket1;
    }

    b_count0++;
    for (; b_count0>8; b_nbucket0++){
        int j = b_nbucket0;
        assert(j<b_nbucket1);
        b_count0 -= b_buckets[j].find_nodes(items, false);
    }
    if (index < b_nbucket0){
        b_count0--;
    }
    return b_buckets[index].update_contact(in, contacted);
}

int
ktable::setkadid(const char kadid[20])
{
    memcpy(b_tableid, kadid, 20);
}

int
ktable::getkadid(char kadid[20])
{
    memcpy(kadid, b_tableid, 20);
    return 0;
}

ktable::ktable()
    :b_count0(0)
{
    b_nbucket0 = 0;
    b_nbucket1 = 1;
    b_buckets = new kbucket[160];
}

ktable::~ktable()
{
    delete[] b_buckets;
}

void
ktable::dump()
{
    int i;
    printf("dump routing table: %u %u\n",
            b_nbucket0, b_nbucket1);
    for (i=0; i<b_nbucket1; i++){
        printf("bucket: %d\n", i);
        b_buckets[i].dump();
    }
    printf("dump ended\n");
}
