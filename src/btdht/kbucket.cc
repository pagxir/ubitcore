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

void
kbucket::dump()
{
    int i;
    kitem_t nodes[_K];
    int count = find_nodes(nodes);
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
