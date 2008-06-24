/* $Id:$ */
#include <stdio.h>
#include <string>

#include "btcodec.h"
#include "bpeermgr.h"

int
bload_peer(const char *buffer, size_t count)
{
    int error;
    size_t size;
    btcodec codec;
    codec.bload(buffer, count);
    int interval = codec.bget().bget("interval").bget(&error);
    if (error != -1){
        printf("interval: %d\n", interval);
    }
    int done_count = codec.bget().bget("done peers").bget(&error);
    if (error != -1){
        printf("done peers: %d\n", done_count);
    }
    int num_peers = codec.bget().bget("num peers").bget(&error);
    if (error != -1){
        printf("num peers: %d\n", done_count);
    }
    int incomplete = codec.bget().bget("incomplete").bget(&error);
    if (error != -1){
        printf("incomplete: %d\n", incomplete);
    }
    const char *failure = codec.bget().bget("failure reason").c_str(&size);
    if (failure != NULL){
        std::string f(failure, size);
        printf("failure reason: %s\n", f.c_str());
    }
    return 0;
}
