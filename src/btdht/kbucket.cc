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

void
kbucket::touch()
{
    b_last_seen = time(NULL);
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
kbucket::update_backup(const kitem_t *in)
{
    int i;

    for (i=0; i<b_nbackup; i++){
        if (b_backups[i].cmpid(in->kadid) == 0){
            b_backups[i].touch();
            return 0;
        }
    }
    if (b_nbackup < 8){
        int r = b_nbackup++;
        b_backups[r] = knode(in->kadid, in->host, in->port);
        b_backups[r].touch();
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
    b_backups[idx] = knode(in->kadid, in->host, in->port);
    b_backups[idx].touch();
    return 0;
}

int
kbucket::update_contact(const kitem_t *in, bool contacted)
{
    int i;
    touch();
    /* aready in this bucket */
    for (i=0; i<b_nknode; i++){
        if (0==b_knodes[i].cmpid(in->kadid)){
            b_knodes[i].set(in);
            contacted&&b_knodes[i].touch();
            return 0;
        }
    }
    /* bucket not full */
    if (b_nknode < _K){
        int idx = b_nknode++;
        b_knodes[idx] = knode(in->kadid, in->host, in->port);
        contacted&&b_knodes[idx].touch();
        return 0;
    }
    /* replace bad node */
    for (i=0; i<b_nknode; i++){
        if (b_knodes[i]._isvalidate() == false){
            b_knodes[i] = knode(in->kadid, in->host, in->port);
            contacted&&b_knodes[i].touch();
            return 0;
        }
    }
    int ping_count = 0;
    for (i=0; i<b_nknode; i++){
        if (b_knodes[i]._isgood() == false){
            ping_count ++;
            break;
        }
    }
    if (ping_count == 0){
        /* all node is good, clear backup node */
        b_nbackup = 0;
    }else if (contacted==true){
        /* add to backup node */
        update_backup(in);
    }
    return 0;
}

int
kbucket::get_ping(kitem_t *item)
{
    int i;
    int found = -1;
    time_t last = time(NULL);
    b_need_ping = false;
    for (i=0; i<b_nknode; i++){
        if (!b_knodes[i]._isgood() && b_knodes[i]._isvalidate()){
            if (last > b_knodes[i].last_seen()){
                last = b_knodes[i].last_seen();
                b_need_ping = true;
                b_knodes[i].get(item);
                found = i;
            }
        }
    }
    return found;
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
    for (i=0; i<b_nbackup; i++){
        b_backups[i].get(&node);
        printf("\tbk: %s %d ", idstr(node.kadid), node.failed);
        printf("%s:%d ", inet_ntoa(*(in_addr*)&node.host),
                htons(node.port));
        printf("\n");
    }
}
