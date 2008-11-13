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
    :b_nknode(0), b_last_seen(0)
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
    for (i=0; i<b_nknode; i++){
        if (b_knodes[i]->cmpid(node->kadid) ==0){
            b_knodes[i]->failed();
            return 0;
        }
    }
    int count = b_nbackup;
    b_nbackup = 0;
    for (i=0; i<count; i++){
        if (b_backups[i]->cmpid(node->kadid) ==0){
            delete b_backups[i];
            continue;
        }
        int idx = b_nbackup++;
        if (idx < i){
            b_backups[idx] = b_backups[i];
        }
    }
    return 0;
}

int
kbucket::find_nodes(kitem_t nodes[], bool validate)
{
    int i;
    int count = 0;
    assert(b_nknode <= _K);
    for (i=0; i<b_nknode; i++){
        if (!validate || b_knodes[i]->_isvalidate()){
            b_knodes[i]->get(&nodes[count]);
            count++;
        }
    }
    return count;
}

int
kbucket::update_contact(const kitem_t *in, bool contacted)
{
    int i;
    int result = 0;
    knode *kn;
    knode *kkn = NULL;
    kitem_t tout;
    time_t  now = time(NULL);
    time_t  lru = now;
    for (i=0; i<b_nknode; i++){
        kn = b_knodes[i];
        if (0==kn->cmpid(in->kadid)){
            kn->set(in);
            contacted&&kn->touch();
            return 0;
        }
        if (kn->_isvalidate() == false){
            kkn = kn;
        }
    }
    if (b_nknode < _K){
        kn  = new knode(in->kadid, in->host, in->port);
        contacted&&kn->touch();
        b_knodes[b_nknode++] = kn;
        return 0;
    }
    if (kkn != NULL){
        kkn->set(in);
        return 0;
    }
    return 0;
}

void
kbucket::dump()
{
    int i;
    kitem_t node;
    for (i=0; i<b_nknode; i++){
        b_knodes[i]->get(&node);
        printf("\t%02x: %s %d ", i, idstr(node.kadid), node.failed);
        printf("%s:%d ", inet_ntoa(*(in_addr*)&node.host),
                htons(node.port));
        printf("\n");
    }
}
