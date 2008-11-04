#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>

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
kbucket::get_knode(knode nodes[])
{
    memcpy(nodes, b_knodes, sizeof(knode)*b_count);
    return b_count;
}

static kbucket *__static_routing_table[160] = {NULL};

int
get_knode(char target[20], knode nodes[], bool valid)
{
    int count = -1;
    int i = get_kbucket_index(target);
    assert(i > 0);
    kbucket *bucket = __static_routing_table[i];
    if (bucket != NULL){
        count = bucket->get_knode(nodes);
    }
    return count;
}
