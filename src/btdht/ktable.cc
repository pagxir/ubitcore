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
    const char *kadid = in->kadid;
    int nbucket1 = bit1_index_of(kadid);
    int index = nbucket1++;

    if (nbucket1 > 160){
#if 0
        printf("internal error: %s %s %s:%d!\n", idstr(kadid), idstr(b_tableid),
                inet_ntoa(*(in_addr*)&in->host), htons(in->port));
#endif
        return 0;
    }

    if (nbucket1 > b_nbucket1){
        b_nbucket1 = nbucket1;
    }

    b_last_seen = time(NULL);
    int val = b_buckets[index].update_contact(in, contacted);
    if (b_buckets[index].need_ping()){
        b_need_ping = true;
    }
    int count = 0;
    for (int i=b_nbucket0; i<b_nbucket1; i++){
        count += b_buckets[i].size();
    }
    while (count > 8){
        count -= b_buckets[b_nbucket0++].size();
    }
    return val;
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
    : b_need_ping(false)
{
    b_nbucket0 = 0;
    b_nbucket1 = 1;
    b_buckets = new kbucket[160];
}

int
ktable::size_of_bucket(int index)
{
    return b_buckets[index].size();
}

int
ktable::get_ping(kitem_t items[], size_t size)
{
    int i;
    int count = 0;

    if (size == 0){
        return 0;
    }

    b_need_ping = false;
    for (i=0; i<b_nbucket1&&count<size; i++){
        if (b_buckets[i].need_ping()){
            count += b_buckets[i].get_ping(&items[count], size-count);
            b_need_ping = true;
        }
    }
    return count;
}

ktable::~ktable()
{
    delete[] b_buckets;
}

void
ktable::dump()
{
    int i;
    printf("dump routing table: %u %u \n",
            b_nbucket0, b_nbucket1);
    for (i=0; i<b_nbucket1; i++){
        printf("%02xbucket@%s:\n", i,
                b_buckets[i].need_ping()?"PING":"WAIT");
        b_buckets[i].dump();
    }
    printf("dump ended: %s\n", idstr(b_tableid));
}
