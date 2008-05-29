#include <stdio.h>
#include <time.h>
#include <winsock.h>
#include <assert.h>
#include <queue>
#include "dht_throttle.h"
#include "fhandle.h"
#include "fsocket.h"
#include "upeer.h"
#include "netmgr.h"
#include "session.h"
#include "peer.h"
#include "tracke.h"
#include "speedup.h"
#include "listener.h"
#include "btlink.h"
#include <signal.h>

void image_checksum();
int load_torrent(const char *path);
int do_random_access_read_bench();

static int __cancel_request = 0;
void request_cancel(int code)
{
    printf("download aborted!\n");
	void save_config();
	save_config();
    __cancel_request = 1;
}

int main(int argc, char *argv[])
{
    int i;

    /*int error = 0; */
    fHandle *pfhList[100];
    for (i = 1; i < argc; i++) {
        load_torrent(argv[i]);
        image_checksum();
    }
    for (i = 0; i < 100; i++) {
        pfhList[i] = new btlink_process();
        pfhList[i]->Wakeup();
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, request_cancel);
    //WakeupListener();
    //WakeupTracke();
    WakeupSpeedup();
    dht_throttle_start();
    fHandle *handle = NULL;
    while (-1!=fHandle::getNextReady(&handle)
            && __cancel_request==0) {
        if (handle->RunActive() == -1) {
            WouldCallAgain(handle);
        }
        ClearCallAgain();
    }
    for (i = 0; i < 100; i++) {
        delete pfhList[i];
    }
	void save_config();
	save_config();
    printf("exiting now ......\n");
    return 0;
}
