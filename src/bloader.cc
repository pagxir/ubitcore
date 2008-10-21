/* $Id$ */
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <string>
#include <sys/stat.h>
#include <fstream>
#include <queue>

#include "sha1.h"
#include "butils.h"
#include "bthread.h"
#include "biothread.h"
#include "btcodec.h"
#include "bclock.h"
#include "bsocket.h"
#include "btdht/bdhtnet.h"

#ifndef NDEBUG
#include "bsocket.h"
#endif


int
btseed_load(const char *buf, int len)
{
    int i;
    int err;
    size_t eln;
    btcodec codec;
    unsigned char digest[20];
    codec.bload(buf, len);
    const char *info = codec.bget().bget("info").b_str(&eln);
    SHA1Hash(digest, (unsigned char*)info, eln);
    set_info_hash(digest);

#if 1
    bentity &nodes = codec.bget().bget("nodes");
    for (i=0; nodes.bget(i).b_str(&eln); i++){
        int port = nodes.bget(i).bget(1).bget(&err);
        const char* ip = nodes.bget(i).bget(0).c_str(&eln);
        std::string ips(ip, eln);
        bdhtnet_node(ips.c_str(), port);
    }
#endif
    return 0;
}

int
genclientid(char ident[20])
{
    int i;
    srand(time(NULL));
    for (i=0; i<20; i++){
        ident[i] = rand()%256;
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    int i;
    char ident[20];
    genclientid(ident);
    setclientid(ident);
    signal(SIGPIPE, SIG_IGN);
    for (i=1; i<argc; i++){
        std::string btseed;
        std::ifstream ifs(argv[i], std::ios::in|std::ios::binary);
        std::copy(std::istreambuf_iterator<char>(ifs),
                std::istreambuf_iterator<char>(),
                std::back_inserter(btseed));
        btseed_load(btseed.c_str(), btseed.size());
    }

    srand(time(NULL));
    bdhtnet_start();

#ifndef DEFAULT_TCP_TIME_OUT
    /* NOTICE: Keep this to less socket connect timeout work ok! */
    bclock c("socket connect clock", 7);
    c.bwakeup(); 
#endif
    bthread *j;
    time_t timeout;
    while (-1 != bthread::bpoll(&j, &timeout)){
        bwait_cancel();
        if (-1 == j->bdocall(timeout)){
            j->bwait();
        }
    }
    return 0;
}
