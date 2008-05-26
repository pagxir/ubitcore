#include <stdio.h>
#include <time.h>
#include <queue>
#include <set>
#include <list>
#include <assert.h>
#include <winsock.h>
#include "fhandle.h"
#include "fsocket.h"
#include "upeer.h"
#include "netmgr.h"
#include "session.h"
#include "peer.h"
#include "tracke.h"
#include "listener.h"
#include "speedup.h"

struct PointerEq
{
    bool operator()(const PeerProcess * lp, const PeerProcess *rp) {
        return lp>rp;
    }
};

static std::set<PeerProcess*, PointerEq> __insched;
static std::queue<PeerProcess*> __schedlist;
static std::queue<PeerProcess*> __schedlist_first;

class SpeedupProcess: public fHandle {
public:
    SpeedupProcess();
    int Abort();
    int RunActive();

private:
    int activeTime;
    int nextOptime;
};

SpeedupProcess::SpeedupProcess()
{
    activeTime = time(NULL);
    nextOptime = activeTime;
}

int SpeedupProcess::Abort()
{
    printf("SpeedupProcess is aborted!\n");
    return 0;
}

extern int do_speedup();
extern int do_speedup1();
extern int do_speedup2();

int SpeedupProcess::RunActive()
{
    int error = WaitTime(activeTime);
    while (error != -1) {
        activeTime += 10;
        do_speedup();
        if (activeTime >= nextOptime) {
            nextOptime += 30;
            do_speedup1();
        }
        do_speedup2();
        error = WaitTime(activeTime);
    }
    return error;
}

int addToChockSched(PeerProcess *peer)
{
    if (__insched.find(peer) == __insched.end()) {
        __schedlist_first.push(peer);
        __insched.insert(peer);
    }
    return 0;
}

static PeerProcess *lastPeer = NULL;

int do_speedup1()
{
    if (lastPeer != NULL) {
        printf("chock: %p\n", lastPeer);
        lastPeer->Chock();
    }
    while (!__schedlist_first.empty()) {
        PeerProcess *p = __schedlist_first.front();
        __schedlist_first.pop();
        if (p->IsActive()) {
            if (p->IsRemoteInterested()) {
                p->Unchock();
                lastPeer = p;
                __schedlist.push(p);
                return 0;
            } else {
                __schedlist.push(p);
            }
        } else {
            __insched.erase(p);
        }
    }
    static PeerProcess __static_marker;
    __schedlist.push(&__static_marker);
    PeerProcess *p = __schedlist.front();
    __schedlist.pop();
    while (p!=&__static_marker) {
        if (p->IsActive()) {
            if (p->IsRemoteInterested()) {
                p->Unchock();
                lastPeer = p;
                __schedlist.push(p);
                return 0;
            } else {
                __schedlist.push(p);
            }
        } else {
            __insched.erase(p);
        }
        p = __schedlist.front();
        __schedlist.pop();
    }
    if (!__schedlist.empty()) {
        p = __schedlist.front();
        __schedlist.pop();
        p->Unchock();
        lastPeer = p;
        __schedlist.push(p);
    }
    return 0;
}

int do_speedup2()
{
    int i;
    int remote_unchock_count = 0;
    int remote_interest_count = 0;
    int local_unchock_count = 0;
    int local_interest_count = 0;
    static PeerProcess *__unchockArray[4];
    std::set<PeerProcess*, PointerEq>::iterator per,start = __insched.begin(),
            end = __insched.end();
    for (i=0; i<4; i++) {
        if (__unchockArray[i] != NULL) {
            __unchockArray[i]->Chock();
            if (__unchockArray[i]->IsActive() == 0) {
                __unchockArray[i] = NULL;
            }
        }
    }
    int active_count = __insched.size();
    for (per=start; per!=end; per++) {
        PeerProcess *chock = *per;
        if (chock->IsActive()) {
            chock->NextRing();
            int limit = chock->IsRemoteChock()?3*1024:1024;
            if (chock->IsInterest()==0 || chock->getSpeed()<limit ) {
                if (active_count>70) {
                    chock->Abort();
                    active_count--;
                }
            }
            local_interest_count += (chock->IsInterest()>0);
            remote_interest_count += (chock->IsRemoteInterested()>0);
            remote_unchock_count += (chock->IsRemoteChock()==0);
        }
        if (chock->IsActive()==0
                || chock==lastPeer) {
            continue;
        }
#define SKIP_ON_OK(a) \
        if (chock == __unchockArray[a]){\
            continue;\
        }
        SKIP_ON_OK(0);
        SKIP_ON_OK(1);
        SKIP_ON_OK(2);
        SKIP_ON_OK(3);
#undef SKIP_ON_OK
        for (i=0; i<4; i++) {
            if (__unchockArray[i] == NULL) {
                __unchockArray[i] = chock;
                break;
            }
            if (chock->getSpeed()>__unchockArray[i]->getSpeed()+512
                    || __unchockArray[i]->IsInterest()==0) {
                PeerProcess *keep = chock;
                chock = __unchockArray[i];
                __unchockArray[i] = keep;
            }
        }
    }
    for (i=0; i<4; i++) {
        if (__unchockArray[i] != NULL) {
            __unchockArray[i]->Unchock();
        }
    }
    if (lastPeer != NULL) {
        lastPeer->Unchock();
    }
    printf("\nremote: U%d I%d local: I%d S%d\n",
           remote_unchock_count, remote_interest_count,
		   local_interest_count, get_comunicate_count());
    void clean_transfer();
    clean_transfer();
    return 0;
}

SpeedupProcess __static_speedup;

int WakeupSpeedup()
{
    __static_speedup.Wakeup();
    return 0;
}

static PeerProcess *__transfer_peers[4];
void clean_transfer()
{
	int i;
	for (i=0; i<4; i++){
		__transfer_peers[i] = NULL;
	}
}

int add_transfer_peer(PeerProcess *peer)
{
	int i;
	for (i=0; i<4; i++){
		if (__transfer_peers[i]==peer){
			return 1;
		}
	}
	for (i=0; i<4; i++){
		if (__transfer_peers[i]==NULL){
			__transfer_peers[i] = peer;
			return 1;
		}
	}
	return 0;
}
