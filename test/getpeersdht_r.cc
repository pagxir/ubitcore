
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <map>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "btcodec.h"

static char __get_peers_struct[] =  {
        "d1:ad2:id20:000000000000000000009:info_hash"
            "20:mnopqrstuvwxyz123456e1:q9:get_peers1:t1:X1:y1:qe"
};

const char *strid(const char *id)
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

const char *idstr(const char *id)
{
    int i;
    static char __idstr[41];
    for (i=0; i<20; i++){
        sprintf(__idstr+2*i, "%02x", id[i]&0xff);
    }
    return __idstr;
}

struct pair_addr{
    in_addr_t addr;
    in_port_t port;
};

bool operator < (const pair_addr &a, const pair_addr &b)
{
    if (a.addr != b.addr){
        return a.addr < b.addr;
    }
    return a.port < b.port;
}

struct kadid{
    uint8_t name[20];
    kadid(const char *a, const char *b);
};

kadid::kadid(const char *a, const char *b)
{
    int i;
    for (i=0; i<20; i++){
        name[i] = a[i]^b[i];
    }
}

bool operator < (const kadid &a, const kadid &b)
{
    return memcmp(a.name, b.name, 20)<0;
}

static int __index = 0;
static int __count = 0;
static char __target[20];
static int __incoming = 0;
static kadid __last_kadid(__target, __target);
static std::map<kadid, int> __kadid_map;
static std::map<kadid, int> __kadid_inmap;
static std::map<pair_addr, int> __pairaddr_map;
static std::map<pair_addr, int> __peeraddr_map;
static std::map<kadid, pair_addr> __address_list;

void find_dump(const char *buff, size_t len,
        struct sockaddr_in *addr, socklen_t addrlen)
{
    int i;
    size_t tlen = 0;
    btcodec codec;
    printf("recv packet: %d\n", len);
    codec.bload(buff, len);
    const char *ident = codec.bget().bget("r").bget("id").c_str(&tlen);
    if (ident == NULL){
        return;
    }
    __incoming++;
    kadid id(ident, __target);
    __kadid_inmap.insert(std::make_pair(id, 1));
    if (__kadid_inmap.size() > 7){
        std::map<kadid, int>::iterator last = --__kadid_inmap.end();
        __last_kadid = last->first;
        __kadid_inmap.erase(last);
    }
    printf("ident: %s\n", idstr(ident));
    const char *token = codec.bget().bget("r").bget("token").c_str(&tlen);
    if (token == NULL){
        return;
    }
    printf("token: ");
    for (i=0; i<tlen; i++){
        printf("%02x", token[i]&0xff);
    }
    printf("\n");

    int idx = 0;
    const char *peers = codec.bget().bget("r").bget("values").bget(idx++).c_str(&tlen);
    while (peers != NULL){
        typedef char peer_t[6];
        peer_t *peer_iter, *peer_end = (peer_t*)(peers+tlen);
        for (peer_iter=(peer_t*)peers; peer_iter<peer_end; peer_iter++){
            pair_addr addr;
            memcpy(&addr.addr, &(*peer_iter)[0], 4);
            memcpy(&addr.port, &(*peer_iter)[4], 2);
            if (__peeraddr_map.insert(std::make_pair(addr, 1)).second){
                printf("peer: %s:%d\n", inet_ntoa(*(in_addr*)&(*peer_iter)[0]),
                        htons(*(in_port_t*)&(*peer_iter)[4]));
            }
        }
        peers = codec.bget().bget("r").bget("values").bget(idx++).c_str(&tlen);
    }

    const char *nodes = codec.bget().bget("r").bget("nodes").c_str(&tlen);
    if (nodes == NULL){
        return;
    }
    in_addr_t naddr; in_port_t nport;
    typedef char compat_t[26];
    compat_t *iter, *end  = (compat_t*)(nodes+tlen);
    for (iter=(compat_t*)nodes; iter<end; iter++){
        pair_addr addr;
        memcpy(&addr.addr, &(*iter)[20], 4);
        memcpy(&addr.port, &(*iter)[24], 2);
        if (__pairaddr_map.insert(std::make_pair(addr, 1)).second){
            kadid id = kadid(__target, &(*iter)[0]);
            if (__kadid_map.insert(std::make_pair(id, 1)).second){
                printf("nodeid: %s\n", idstr(&(*iter)[0]));
                printf("address: %s:%d\n",
                        inet_ntoa(*(in_addr*)&(*iter)[20]),
                htons(*(in_port_t*)&(*iter)[24]));
                __address_list.insert(std::make_pair(id, addr));
                __count++;
            }
        }
    }

    return;
}


int
out_packet(int udp, char *buff, size_t size)
{
    struct sockaddr_in remote;

    std::map<kadid, pair_addr>::iterator iter = __address_list.begin();
    
    if (iter == __address_list.end()){
        __index = __count;
        return 0;
    }
    
    if (__incoming>8 && __last_kadid<iter->first){
        __index = __count;
        return 0;
    }

    pair_addr addr = iter->second;
    __address_list.erase(iter);
    remote.sin_family = AF_INET;
    remote.sin_port = addr.port;
    remote.sin_addr.s_addr = addr.addr;
    memcpy(buff, __get_peers_struct, strlen(__get_peers_struct));
    memcpy(&buff[46], __target, 20);
    sendto(udp, buff, strlen(__get_peers_struct), 0,
            (struct sockaddr*)&remote, sizeof(remote));
    return 0;
}

int
in_packet(int udp, char *buff, size_t size)
{
    struct sockaddr_in sender;
    socklen_t senderlen = sizeof(sender);
    int error = recvfrom(udp, buff, size, 0,
            (struct sockaddr*)&sender, &senderlen);
    find_dump(buff, error, &sender, sizeof(sender));
    return 0;
}

int main(int argc, char *argv[])
{
    char buffer[8192];
    int udp = socket(AF_INET, SOCK_DGRAM, 0);

    if (argc < 4){
        fprintf(stderr, "too few argument\n");
        return -1;
    }

    memcpy(__target, strid(argv[1]), 20); 
    for (int i=2; i<argc; i+=2){
        pair_addr addr;
        addr.addr = inet_addr(argv[i]);
        addr.port = htons(atoi(argv[i+1]));
        char idbuff[20];
        memcpy(idbuff, &addr, sizeof(addr));
        kadid id = kadid(idbuff, __target);
        __address_list.insert(std::make_pair(id, addr));
        if (__pairaddr_map.insert(std::make_pair(addr, 1)).second){
            __address_list.insert(std::make_pair(id, addr));
            __count++;
        }
    }

    struct timeval tv;

    int out=0, second, count;
    fd_set readfds;

    for (;__index<__count||out>0;){
        second = 5;
        if (__index<__count && out<3){
            second = 0;
            out_packet(udp, buffer, sizeof(buffer));
            out++;
        }

        FD_ZERO(&readfds);
        FD_SET(udp, &readfds);
        tv.tv_sec = second;
        tv.tv_usec = 3000;
        count = select(udp+1, &readfds,
                NULL, NULL, &tv);

        if (count==0 && second==5){
            /* sll timeout */
            out = 0;
        }

        if (count>0 && out>0){
            in_packet(udp, buffer, sizeof(buffer));
            out--;
        }
    }

    close(udp);
    return 0;
}
