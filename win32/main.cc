/* $Id$ */
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <string>
#include <fstream>
#include <queue>

#include "sha1.h"
#include "butils.h"
#include "bthread.h"
#include "biothread.h"
#include "bdhtnet.h"
#include "btcodec.h"
#include "bclock.h"
#include "bqueue.h"
#include "btracker.h"
#include "boffer.h"
#include "bupdown.h"
#include "bchunk.h"
#include "bfiled.h"

#ifndef NDEBUG
#include "bsocket.h"
#endif


burlthread*
bttracker_start(const char *url, const unsigned char info_hash[20],
        const unsigned char peer_id[20])
{
    int i;

    static const char *http_encode[256] = {
        "%00","%01","%02","%03","%04","%05","%06","%07",
        "%08","%09","%0a","%0b","%0c","%0d","%0e","%0f",
        "%10","%11","%12","%13","%14","%15","%16","%17",
        "%18","%19","%1a","%1b","%1c","%1d","%1e","%1f",
        "%20","%21","%22","%23","%24","%25","%26","%27",
        "%28","%29","%2a","%2b","%2c","%2d","%2e","%2f",
        "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
        "8",  "9",  "%3a","%3b","%3c","%3d","%3e","%3f",
        "%40","A",  "B",  "C",  "D",  "E",  "F",  "G",
        "H",  "I",  "J",  "K",  "L",  "M",  "N",  "O",
        "P",  "Q",  "R",  "S",  "T",  "U",  "V",  "W",
        "X",  "Y",  "Z",  "%5b","%5c","%5d","%5e","%5f",
        "%60","a",  "b",  "c",  "d",  "e",  "f",  "g",
        "h",  "i",  "j",  "k",  "l",  "m",  "n",  "o",
        "p",  "q",  "r",  "s",  "t",  "u",  "v",  "w",
        "x",  "y",  "z",  "%7b","%7c","%7d","%7e","%7f",
        "%80","%81","%82","%83","%84","%85","%86","%87",
        "%88","%89","%8a","%8b","%8c","%8d","%8e","%8f",
        "%90","%91","%92","%93","%94","%95","%96","%97",
        "%98","%99","%9a","%9b","%9c","%9d","%9e","%9f",
        "%a0","%a1","%a2","%a3","%a4","%a5","%a6","%a7",
        "%a8","%a9","%aa","%ab","%ac","%ad","%ae","%af",
        "%b0","%b1","%b2","%b3","%b4","%b5","%b6","%b7",
        "%b8","%b9","%ba","%bb","%bc","%bd","%be","%bf",
        "%c0","%c1","%c2","%c3","%c4","%c5","%c6","%c7",
        "%c8","%c9","%ca","%cb","%cc","%cd","%ce","%cf",
        "%d0","%d1","%d2","%d3","%d4","%d5","%d6","%d7",
        "%d8","%d9","%da","%db","%dc","%dd","%de","%df",
        "%e0","%e1","%e2","%e3","%e4","%e5","%e6","%e7",
        "%e8","%e9","%ea","%eb","%ec","%ed","%ee","%ef",
        "%f0","%f1","%f2","%f3","%f4","%f5","%f6","%f7",
        "%f8","%f9","%fa","%fb","%fc","%fd","%fe","%ff"
    };

    std::string bturl(url);
    if (-1 == bturl.find('?')){
        bturl += "?info_hash=";
    }else{
        bturl += "&info_hash=";
    }
    for (i=0; i<20; i++){
        bturl += http_encode[info_hash[i]];
    }
    bturl += "&peer_id=";
    for (i=0; i<20; i++){
        bturl += http_encode[peer_id[i]];
    }
    bturl += "&port=15180";
    bturl += "&uploaded=0";
    bturl += "&downloaded=0";
    bturl += "&left=732854576";
#if 0
    bturl += "&event=started";
#endif
    bturl += "&key=1785265";
    bturl += "&compact=1";
    bturl += "&numwant=200";
    printf("T: %s\n", bturl.c_str());
    burlthread *get = new burlthread(bturl.c_str(), 300);
    get->bwakeup();
    return get;
}

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
    const char *list = codec.bget().bget("announce-list").b_str(&eln);
    if (list != NULL){
        const char *urlbuf = NULL;
        bentity& en = codec.bget().bget("announce-list");
        for (i=0;urlbuf=en.bget(i).bget(0).c_str(&eln);i++){
            std::string url(urlbuf, eln);
            bttracker_start(url.c_str(), digest, digest);
        }
    }else{
        const char *urlbuf = codec.bget().bget("announce").c_str(&eln);
        std::string url(urlbuf, eln);
        bttracker_start(url.c_str(), digest, digest);
    }

    int piecel = codec.bget().bget("info").bget("piece length").bget(&err);
    assert(err!=-1 && piecel>0);
    int bits = 0;
    int bcount, brest;
    for (bits=0; bits<32; bits++){
        if (piecel&(1<<bits)){
            assert((1<<bits)==piecel);
            break;
        }
    }

    const char *name = codec.bget().bget("info").bget("name").c_str(&eln);
    std::string str_name(name, eln);
    printf("name: %s\n", str_name.c_str());

    int length = codec.bget().bget("info").bget("length").bget(&err);
    const char *files = codec.bget().bget("info").bget("files").b_str(&eln);
    assert((err==-1)^(files==NULL));
    if (err != -1){
        codec.bget().bget("info").bget("length").b_val(&bcount, &brest, bits);
        badd_per_file(bcount, brest);
    }
    if (files != NULL){
        bcount=0, brest=0;
        int count, rest;
        bentity &bfiles = codec.bget().bget("info").bget("files");
        for (i=0; bfiles.bget(i).b_str(&eln); i++){
            err = bfiles.bget(i).bget("length").b_val(&count, &rest, bits);
            assert(err != -1);
            bcount += count;
            brest += rest;
            badd_per_file(bcount, brest);
        }
        bcount += (brest>>bits);
        brest %= piecel;
    }
    printf("piece info: %dx%d %d\n", bcount, piecel, brest);
    const char *pieces = codec.bget().bget("info").bget("pieces").c_str(&eln);
    assert(pieces!=NULL && (eln==20*(bcount+(brest>0))));
    bset_piece_info(piecel, bcount, brest);

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
gen_peer_ident(unsigned char ident[20])
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
    unsigned char ident[20];
    gen_peer_ident(ident);
    set_peer_ident(ident);
    //signal(SIGPIPE, SIG_IGN);
    std::queue<burlthread*> burlqueue;
    for (i=1; i<argc; i++){
        if (strncmp(argv[i], "http://", 7)==0){
            burlthread *get = new burlthread(argv[i], 30);
            burlqueue.push(get);
            get->bwakeup();
        }else{
            std::string btseed;
            std::ifstream ifs(argv[i], std::ios::in|std::ios::binary);
            std::copy(std::istreambuf_iterator<char>(ifs),
                    std::istreambuf_iterator<char>(),
                    std::back_inserter(btseed));
            btseed_load(btseed.c_str(), btseed.size());
        }
    }

    bqueue bcq[25];
    for (i=0; i<5; i++){
        bcq[i].bwakeup();
    }

    bupdown *updown[200];
    for (i=0; i<200; i++){
        updown[i] = new bupdown();
        updown[i]->bwakeup();
    }

    srand(time(NULL));
    boffer_start(0);
    bfiled_start(30);

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
    while (!burlqueue.empty()){
        delete burlqueue.front();
        burlqueue.pop();
    }
    return 0;
}
