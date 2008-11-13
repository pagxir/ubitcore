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
    :b_nknode(0), b_last_seen(0), b_nbackup(0)
{
    b_knodes = new knode[_K];
    b_backups = new knode[_K];
}

kbucket::~kbucket()
{
    delete[] b_knodes;
    delete[] b_backups;
}

void
kbucket::touch()
{
    b_last_seen = time(NULL);
}

int
kbucket::failed_contact(const kitem_t *node)
{
    int i, r=-1, b=-1;
    for (i=0; i<b_nknode; i++){
        if (b_knodes[i].cmpid(node->kadid) ==0){
            b_knodes[i].failed();
            if (!b_knodes[i]._isvalidate()){
                r = i;
            }
            break;
        }
    }
    int count = b_nbackup;
    b_nbackup = 0;
    time_t last = 0;
    for (i=0; i<count; i++){
        if (b_backups[i].cmpid(node->kadid) ==0){
            continue;
        }
        int idx = b_nbackup++;
        if (idx < i){
            b_backups[idx] = b_backups[i];
        }
        if (b_backups[idx].last_seen() > last){
            last = b_backups[idx].last_seen();
            b = idx;
        }
    }
    if (r!=-1 && b!=-1){
        b_knodes[r] = b_backups[b];
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
        if (!validate || b_knodes[i]._isvalidate()){
            b_knodes[i].get(&nodes[count]);
            count++;
        }
    }
    return count;
}

int
kbucket::update_contact(const kitem_t *in, bool contacted)
{
    int i, n=-1;
    int result = 0;
    knode *kn;
    time_t  now = time(NULL);
    for (i=0; i<b_nknode; i++){
        kn = &b_knodes[i];
        if (0==kn->cmpid(in->kadid)){
            kn->set(in);
            contacted&&kn->touch();
            return 0;
        }
        if (kn->_isvalidate() == false){
            n = i;
        }
    }
    if (b_nknode < _K){
        int idx = b_nknode++;
        b_knodes[idx] = knode(in->kadid, in->host, in->port);
        contacted&&b_knodes[idx].touch();
        return 0;
    }
    if (n != -1){
        b_knodes[i] = knode(in->kadid, in->host, in->port);
        contacted&&kn->touch();
        return 0;
    }
    if (contacted == true){
        int r = 0;
        time_t last = time(NULL);
        for (i=0; i<b_nbackup; i++){
            if (b_backups[i].cmpid(in->kadid) == 0){
                b_backups[i].touch();
                return 0;
            }
            if (b_backups[i].last_seen() < last){
                last = b_backups[i].last_seen();
                r = i;
            }
        }
        if (b_nbackup < 8){
            r = b_nbackup++;
        }
        b_backups[r] = knode(in->kadid, in->host, in->port);
        b_backups[r].touch();
    }
    return 0;
}

void
kbucket::dump()
{
    int i;
    kitem_t node;
    for (i=0; i<b_nknode; i++){
        b_knodes[i].get(&node);
        printf("\t%02x: %s %d ", i, idstr(node.kadid), node.failed);
        printf("%s:%d ", inet_ntoa(*(in_addr*)&node.host),
                htons(node.port));
        printf("\n");
    }
}
