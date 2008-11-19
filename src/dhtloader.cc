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
#include <arpa/inet.h>
#include <fstream>
#include <queue>

#include "sha1.h"
#include "butils.h"
#include "bthread.h"
#include "biothread.h"
#include "bidle.h"
#include "btimerd.h"
#include "btcodec.h"
#include "bclock.h"
#include "bsocket.h"
#include "bidle.h"
#include "netdb.h"
#include "btdht/btkad.h"

#ifndef NDEBUG
#include "bsocket.h"
#endif


const char *
strid(const char *id)
{
    int i;
    uint8_t hex[256];
    static char __strid[20];
    for (i=0; i<10; i++){
        hex['0'+i] = i;
    }
    hex['A'] = 0xa; hex['a'] = 0xa;
    hex['B'] = 0xb; hex['b'] = 0xb;
    hex['C'] = 0xc; hex['c'] = 0xc;
    hex['D'] = 0xd; hex['d'] = 0xd;
    hex['E'] = 0xe; hex['e'] = 0xe;
    hex['F'] = 0xf; hex['f'] = 0xf;
    for (i=0; i<20; i++){
        uint8_t h = (hex[(uint8_t)id[2*i]]<<4);
        h += hex[(uint8_t)id[2*i+1]];
        __strid[i] = h;
    }
    return __strid;
}

int
btseed_load(const char *buf, int len)
{
    int i;
    int err;
    size_t eln;
    btcodec codec;
    uint8_t digest[20];
    codec.bload(buf, len);
    const char *info = codec.bget().bget("info").b_str(&eln);

    if (info != NULL){
        SHA1Hash(digest, info, eln);
        set_info_hash((char*)digest);
    }

#if 1
    bentity &nodes = codec.bget().bget("nodes");
    for (i=0; nodes.bget(i).b_str(&eln); i++){
        int port = nodes.bget(i).bget(1).bget(&err);
        const char* ip = nodes.bget(i).bget(0).c_str(&eln);
        std::string ipstr(ip, eln);
        in_addr_t host = inet_addr(ipstr.c_str());
        add_boot_contact(host, htons(port));
    }
#endif
    return 0;
}

int
genclientid(char ident[20])
{
    int i;
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
    srand(time(NULL));
    genclientid(ident);
    setclientid(ident);
    signal(SIGPIPE, SIG_IGN);
    for (i=1; i<argc; i++){
        if (i > 1){
            tracker_start(strid(argv[i]));
            continue;
        }
        std::string btseed;
        std::ifstream ifs(argv[i], std::ios::in|std::ios::binary);
        std::copy(std::istreambuf_iterator<char>(ifs),
                std::istreambuf_iterator<char>(),
                std::back_inserter(btseed));
        btseed_load(btseed.c_str(), btseed.size());
    }

    biorun();
    bidlerun();
    btimerdrun();
    bdhtnet_start();

#ifndef DEFAULT_TCP_TIME_OUT
    /* NOTICE: Keep this to less socket connect timeout work ok! */
    bclock c("socket connect clock", 7);
    c.bwakeup(NULL); 
#endif
    return 0;
}

int dhtpoll()
{
    bthread *j;
    poll_start();
    while (pollable() && -1!=bthread::bpoll(&j)){
        j->bdocall();
    }
    poll_end();
    return 0;
}
