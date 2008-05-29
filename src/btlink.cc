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
	link = NULL;
}

int btlink_process::RunActive()
{
	for (;;){
	   	if (link==NULL){
		   	link = NetMgr::GetNextSocket();
	   	}
		if (link==NULL){
			return -1;
		}
		if (link->attach(this) == -1){
			link = NULL;
			continue;
		}
	   	if (-1 == link->dosomething()){
			return -1;
	   	}
		link->detach();
	   	link = NULL;
	}
	return 0;
}

int btlink_process::Abort()
{
	printf("aborted: %d!\n", WSAGetLastError());
	perror("");
	if (link != NULL){
		link->Abort();
		link->detach();
	   	link = NULL;
	}
	Wakeup();
	return 0;
}
