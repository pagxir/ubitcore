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

static const char nullid[20] = {0x0};

kbucket::kbucket()
    :b_nknode(0), b_last_seen(0),
    b_nbackup(0), b_need_ping(false)
{
    b_knodes = new knode[_K];
    b_backups = new knode[_K];
}

kbucket::~kbucket()
{
    delete[] b_knodes;
    delete[] b_backups;
}

bool
kbucket::touch()
{
    b_last_seen = time(NULL);
    return 0;
}

int
kbucket::failed_contact(const kitem_t *node)
{
    int i;
    knode *kn = NULL;
    for (i=0; i<b_nknode; i++){
        if (b_knodes[i].cmpid(node->kadid) ==0){
            b_knodes[i].failed();
            kn = &b_knodes[i];
            break;
        }
    }
    int count = b_nbackup;
    b_nbackup = 0;
    for (i=0; i<count; i++){
        if (b_backups[i].cmpid(node->kadid) ==0){
            continue;
        }
        if (b_backups[i].cmpid(nullid) ==0){
            continue;
        }
        int idx = b_nbackup++;
        if (idx < i){
            b_backups[idx] = b_backups[i];
        }
    }
    if (kn!=NULL && b_nbackup>0 && kn->last_seen()+900<time(NULL)){
        /* replace with good node */
        int j, found=0;
        time_t now = 0;
        for (j=0; j<b_nbackup; j++){
            if (b_backups[j].last_seen() > now){
                now = b_backups[j].last_seen();
                found = j;
            }
        }
        *kn = b_backups[found];
        kn->last_seen()&&touch();
        b_backups[found] = knode(nullid, 0, 0);
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
kbucket::update_backup(const kitem_t *in, bool contacted)
{
    int i;

    for (i=0; i<b_nbackup; i++){
        if (b_backups[i].cmpid(in->kadid) == 0){
            contacted&&b_backups[i].touch();
            return 0;
        }
    }
    if (b_nbackup < 8){
        int r = b_nbackup++;
        b_backups[r] = knode(in->kadid, in->host, in->port);
        contacted&&b_backups[r].touch();
        return 0;
    }
    int idx = 0;
    time_t last = time(NULL);
    for (i=0; i<b_nbackup; i++){
        if (b_backups[i].last_seen() < last){
            last = b_backups[i].last_seen();
            idx = i;
        }
    }
    if (contacted || b_backups[idx].last_seen()==0){
        b_backups[idx] = knode(in->kadid, in->host, in->port);
        contacted&&b_backups[idx].touch();
    }
    return 0;
}

int
kbucket::update_contact(const kitem_t *in, bool contacted)
{
    int i;
    /* aready in this bucket */
    for (i=0; i<b_nknode; i++){
        if (0==b_knodes[i].cmpid(in->kadid)){
            b_knodes[i].set(in);
            contacted&&b_knodes[i].touch();
            return contacted&&touch();
        }
    }
    /* bucket not full */
    if (b_nknode < _K){
        int idx = b_nknode++;
        b_knodes[idx] = knode(in->kadid, in->host, in->port);
        contacted&&b_knodes[idx].touch();
        return contacted&&touch();
    }
    /* replace bad node */
    for (i=0; i<b_nknode; i++){
        if (b_knodes[i]._isvalidate() == false){
            b_knodes[i] = knode(in->kadid, in->host, in->port);
            contacted&&b_knodes[i].touch();
            return contacted&&touch();
        }
    }
    int ping_count = 0;
    for (i=0; i<b_nknode; i++){
        if (b_knodes[i]._isdoubt()){
            ping_count ++;
            break;
        }
    }
    if (ping_count == 0){
        /* all node is good, clear backup node */
        b_nbackup = 0;
    }else{
        /* add to backup node */
        update_backup(in, contacted);
        b_need_ping = true;
    }
    return 0;
}

int
kbucket::get_ping(kitem_t items[], size_t size)
{
    int i;
    int count = 0;
    size_t real_size = size>b_nbackup?b_nbackup:size;

    if (size == 0){
        return 0;
    }

    b_need_ping = false;
    for (i=0; i<b_nknode&&count<real_size; i++){
        if (b_knodes[i]._isdoubt()){
            b_knodes[i].get(&items[count]);
            count++;
        }
    }
    return count;
}

void
kbucket::dump()
{
    int i;
    kitem_t node;
    time_t now = time(NULL);
    if (b_nbackup > 0){
        for (i=0; i<b_nbackup; i++){
            if (b_backups[i].last_seen()){
                printf("%4d ", now-b_backups[i].last_seen());
            }
        }
        printf("\n");
    }
    for (i=0; i<b_nknode; i++){
        b_knodes[i].get(&node);
        if (b_knodes[i].last_seen() > 0){
            printf("  %s %2d %4d %4d ", idstr(node.kadid), node.failed,
                    now-b_knodes[i].last_seen(), now-b_knodes[i].birthday());
        }else{
            printf("  %s %2d ival %4d ", idstr(node.kadid), node.failed, 
                    now-b_knodes[i].birthday());
        }
        printf("%s:%d ", inet_ntoa(*(in_addr*)&node.host),
                htons(node.port));
        printf("\n");
    }
}
