#include <string.h>
#include <winsock.h>
#include <stdio.h>
#include <assert.h>
#include <set>
#include <queue>

#include "fhandle.h"
#include "fsocket.h"
#include "upeer.h"
#include "netmgr.h"
#include "peer.h"

struct peer_less{
    bool operator()(const PeerProcess *laddr,
		   	const PeerProcess *raddr)const
   	{
	   	if (laddr->address!=raddr->address) {
		   	return laddr->address > raddr->address;
	   	}
	   	return laddr->port > raddr->port;
   	}
};

struct peer_id_less{
    bool operator()(const PeerProcess *laddr,
		   	const PeerProcess *raddr)const
   	{
		return memcmp(laddr->peer_ident, raddr->peer_ident, 20)>0;
   	}
};

static NetMgr __NetMgr;
static long  __self_address;
static long  __self_port;
static std::set<PeerProcess*, peer_less> __peer_list;
static std::set<PeerProcess*, peer_id_less> __comunicate_peer_list;
static std::queue<PeerProcess*> __peer_queue;

static std::queue<fHandle*> __btlink_queue;
static int CallAgainNextClient(fHandle * handle, int argc, void *argv)
{
    __btlink_queue.push(handle);
    return 0;
}

static int WakeupNextClient(int much)
{
    int    count = 0;
    while (!__btlink_queue.empty() && count != much) {
        __btlink_queue.front()->Wakeup();
        __btlink_queue.pop();
        count++;
    }
    return count;
}

int SetWaitNewClient(int code)
{
    if (code == -1) {
        SetCallAgainHandle(CallAgainNextClient, 0, NULL);
    }
    return code;
}

NetMgr::NetMgr()
{
    fild = -1;
}

int do_speedup()
{
    return 0;
}

int NetMgr::savePeer(unsigned long inaddr, unsigned short port)
{
    if (port != 0 && inaddr != 0) {
        PeerProcess *peer = new PeerProcess();
        peer->port = port;
        peer->address = inaddr;

        if (__peer_list.find(peer) == __peer_list.end()){
			/* add new peer */
			__peer_list.insert(peer);
		}else{
			PeerProcess *kill = peer;
			peer = *__peer_list.find(peer);
			delete kill;
		}
	   	__peer_queue.push(peer);
        WakeupNextClient(1);
    }
    return 0;
}

int    NetMgr::SetActivePeer(int fild_, sockaddr_in * addr, int addrlen)
{
    if (fild == -1 && WakeupNextClient(1) == 0) {
        printf("too much connection!\n");
        closesocket(fild_);
        return -1;
    }
	if (fild!=-1 && fild!=fild_){
	   	closesocket(fild);
	}
    fild = fild_;
    peername = *addr;
    //printf("accepted connection: %d!\n", fild_);
    return 0;
}
#if 0
int    NetMgr::NextClient(PeerSocket * userPeer)
{
    /* int debug = 0; */
    int    wait = 1;
    int    error = __NetMgr.fild;
    __NetMgr.fild = -1;
    PeerProcess *peer = __NetMgr.peername;
    while (error == -1 && !__peer_queue.empty()) {
        peer = __peer_queue.front();
        __peer_queue.pop();
        if (__accept_queue_set.find(peer) == __accept_queue_set.end()) {
            unsigned long    optval = 1;
            error = socket(AF_INET, SOCK_STREAM, 0);
            ioctlsocket(error, FIONBIO, &optval);
            addr.sin_family = AF_INET;
            wait = connect(error, (sockaddr *) & addr, sizeof(addr));
            if (wait == -1 && WSAGetLastError() != EINPROGRESS) {
                closesocket(error);
                error = -1;
            }
        }
        __peer_list.erase(peer);
    }
    if (error != -1) {
        /* sockaddr_in uuaddr; */
        /* int len = sizeof(uuaddr); */
        return userPeer->Attach(error, &addr, sizeof(addr), wait);
    }
    return SetWaitNewClient(error);
}
#endif

int startSession(sockaddr_in * peerName, int addrlen)
{
#if 0
    if (__accept_queue_set.find(*peerName) == __accept_queue_set.end()) {
        __accept_queue_set.insert(*peerName);
        return 0;
    }
    if (peerName->sin_addr.s_addr > (size_t) __self_address) {
        return 0;
    }
    if (peerName->sin_port > __self_port) {
        return 0;
    }
    return -1;
#endif
	return 0;
}

int SetActivePeer(int fild, sockaddr_in * addr, int addrlen)
{
    return __NetMgr.SetActivePeer(fild, addr, addrlen);
}

int getSelfAddress(char *buf, int len)
{
    sockaddr_in    peerName;
    if (len > (int)sizeof(peerName)) {
        peerName.sin_family = AF_INET;
        peerName.sin_port = __self_port;
        peerName.sin_addr.s_addr = __self_address;

#if 0
        printf("Self Address: %s:%d\n", inet_ntoa(peerName.sin_addr),
               htons(__self_port));

#endif                /* */
        memcpy(buf, &peerName, sizeof(peerName));
        return sizeof(peerName);
    }
    return 0;
}

int
setSelfAddress(char *buf, int len)
{
    PeerProcess *self_peer;
    if (len == 8) {
        /* unsigned char *addr = (unsigned char*)buf; */
        memcpy(&__self_port, buf + 4, sizeof(__self_port));
        memcpy(&__self_address, buf, sizeof(__self_address));
		self_peer = new PeerProcess();
        self_peer->address = __self_address;
        self_peer->port = __self_port;
        if (__peer_list.find(self_peer) == __peer_list.end()) {
            __peer_list.insert(self_peer);
        }else{
			delete self_peer;
		}
    }
    return 0;
}

#if 0
int
loadPeerList(char *buf, int len)
{
    char           *peer = buf;
    char           *peerlimit = buf + len;
    for (; peer < peerlimit; peer += 6) {
        unsigned long    inaddr;
        unsigned short    port;
        memcpy(&inaddr, peer, sizeof(inaddr));
        memcpy(&port, peer + sizeof(inaddr), sizeof(port));
        __NetMgr.savePeer(inaddr, port);
    }
    return 0;
}
#endif

int
loadPeers(char *buf, int len)
{
    int        i;
	int        count = 0;
    unsigned long    ipaddr;
    unsigned short    port;
#if 0
    __NetMgr.savePeer(inet_addr("127.0.0.1"), htons(6934));
#endif
#if 1
    for (i = 0; i + 6 < len; i += 6) {
        memcpy(&ipaddr, buf + i, 4);
        memcpy(&port, buf + i + 4, 2);
        __NetMgr.savePeer(ipaddr, port);
		count ++;
    }
	printf("load %d peers!\n", count);
#endif
    return 0;
}

int
AddNodes(const char *value, int port)
{
    __NetMgr.savePeer(inet_addr(value), port);
    return 0;
}

PeerProcess *NetMgr::GetNextPeer()
{
	PeerProcess *peer;
	if (__NetMgr.fild != -1){
		peer = new PeerProcess();
		peer->fildes = __NetMgr.fild;
	    peer->address = __NetMgr.peername.sin_addr.s_addr;
		peer->port = __NetMgr.peername.sin_port;
		__peer_list.insert(peer);
		__NetMgr.fild = -1;
		return peer;
	}
	while (!__peer_queue.empty()){
		peer = __peer_queue.front();
		__peer_queue.pop();
		if (peer->need_active() == 0){
			return peer;
		}
	}
	SetWaitNewClient(-1);
	return NULL;
}

int try_add_peer_id(PeerProcess *peer)
{
	if (__comunicate_peer_list.find(peer) == __comunicate_peer_list.end()){
		__comunicate_peer_list.insert(peer);
	}
	return 0;
}

int get_comunicate_count()
{
	return __comunicate_peer_list.size();
}
