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
        if (b_knodes[i]->cmpid(node->kadid) ==0){
            b_knodes[i]->failed();
            break;
        }
    }
    return 0;
}

int
kbucket::find_nodes(kitem_t nodes[])
{
    int i;
    int count = 0;
    assert(b_count <= _K);
    for (i=0; i<b_count; i++){
        if (b_knodes[i]->_isvalidate()){
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
    assert(b_count <= _K);
    for (i=0; i<b_count; i++){
        b_knodes[i]->getnode(&nodes[count]);
        count++;
    }
    return count;
}

int
kbucket::invalid_node(const kitem_t *node)
{
    int i;
    knode *kn;
    for (i=0; i<b_count; i++){
        kn = b_knodes[i];
        if (kn->cmpid(node->kadid) != 0){
            continue;
        }
        if (kn->cmphost(node->host) != 0){
            printf("warn: host change\n");
            continue;
        }
        if (kn->cmpport(node->port) != 0){
            printf("warn: port change\n");
            continue;
        }
        kn->failed();
    }
    return 0;
}

int
kbucket::update_contact(const kitem_t *in, kitem_t *out, bool contacted)
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
        if (0==kn->cmpid(in->kadid)){
            kn->set(in);
            contacted&&kn->touch();
            return 0;
        }
        if (kn->_isvalidate() == false){
            kkn = kn;
        }
#if 0
        if (tout.atime < lru){
            lru = tout.atime;
            *out = tout;
        }
#endif
    }
    if (b_count < _K){
        kn  = new knode(in->kadid, in->host, in->port);
        contacted&&kn->touch();
        b_knodes[b_count++] = kn;
#if 0
        printf("adding one knode\n");
#endif
        return 0;
    }
    if (kkn != NULL){
#if 0
        printf("set: %d\n", b_count);
#endif
        kkn->set(in);
        return 0;
    }
    //printf("drop: %d\n", b_count);
    return 0;
#if 0
    return (now-lru>15*60);
#endif
}

void
kbucket::dump()
{
    int i;
    kitem_t node;
    for (i=0; i<b_count; i++){
        b_knodes[i]->getnode(&node);
        printf("\t%02x: %s %d ", i, idstr(node.kadid), node.failed);
        printf("%s:%d ", inet_ntoa(*(in_addr*)&node.host),
                htons(node.port));
        printf("\n");
    }
}
