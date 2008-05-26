#include <stdio.h>
#include <time.h>
#include <queue>
#include <winsock.h>
#include <assert.h>
#include "handshake.h"
#include "fhandle.h"
#include "fsocket.h"
#include "upeer.h"
#include "session.h"
#include "peer.h"
#include "netmgr.h"
#include "tracke.h"
#include "listener.h"
#include "zpiece.h"
#include "speedup.h"
#include "btlink.h"


btlink_process::btlink_process()
{
	peer = NULL;
}

int btlink_process::RunActive()
{
	for (;;){
	   	if (peer==NULL){
		   	peer = NetMgr::GetNextPeer();
	   	}
		if (peer==NULL){
			return -1;
		}
		if (peer->attach(this) == -1){
			peer = NULL;
			continue;
		}
	   	if (-1 == peer->RunActive()){
			return -1;
	   	}
		peer->detach();
	   	peer = NULL;
	}
	return 0;
}

int btlink_process::Abort()
{
	printf("aborted: %d!\n", WSAGetLastError());
	perror("");
	if (peer != NULL){
		peer->Abort();
		peer->detach();
	   	peer = NULL;
	}
	Wakeup();
	return 0;
}
