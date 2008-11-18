
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "btcodec.h"

static const char __find_struct[] = {
    "d1:ad2:id20:abcdefghij01234567896:target"
        "20:abcdefghij0123456789e1:q9:find_node1:t4:FFFF1:y1:qe"
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

void find_dump(const char *buff, size_t len,
        struct sockaddr_in *addr, socklen_t addrlen)
{
    size_t tlen = 0;
    btcodec codec;

    printf("recv packet: %d\n", len);
    codec.bload(buff, len);
    const char *ident = codec.bget().bget("r").bget("id").c_str(&tlen);
    if (ident == NULL){
        return;
    }
    printf("ident: %s\n", idstr(ident));
    const char *nodes = codec.bget().bget("r").bget("nodes").c_str(&tlen);
    if (nodes == NULL){
        return;
    }
    in_addr_t naddr; in_port_t nport;
    typedef char compat_t[26];
    compat_t *iter, *end  = (compat_t*)(nodes+tlen);
    for (iter=(compat_t*)nodes; iter<end; iter++){
        printf("nodeid: %s\n", idstr(&(*iter)[0]));
        printf("address: %s:%d\n",
                inet_ntoa(*(in_addr*)&(*iter)[20]),
                htons(*(in_port_t*)&(*iter)[24]));
    }
    return;
}

int main(int argc, char *argv[])
{
    int i;
    int error;
    char buffer[8192];
    struct sockaddr_in remote, sender;
    int udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (argc < 4){
        fprintf(stderr, "too few argument\n");
        return -1;
    }
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0; 
    error = setsockopt(udp, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (error == -1){
        printf("setsockopt");
        return -1;
    }
    remote.sin_family = AF_INET;
    remote.sin_port = htons(atoi(argv[3]));
    remote.sin_addr.s_addr = inet_addr(argv[2]);
    for (i=0; i<3; i++){
        printf("send: %d\n", i);
        memcpy(buffer, __find_struct, strlen(__find_struct));
        memcpy(&buffer[43], strid(argv[1]), 20);
        error = sendto(udp, buffer, strlen(__find_struct), 0,
                (struct sockaddr*)&remote, sizeof(remote));
        if (error == -1){
            perror("sendto");
            return -1;
        }
        socklen_t senderlen = sizeof(sender);
        error = recvfrom(udp, buffer, sizeof(buffer), 0,
                (struct sockaddr*)&sender, &senderlen);
        if (error>0 || errno!=EAGAIN){
            break;
        }
    }

    if (error > 0){
        find_dump(buffer, error, &remote, sizeof(remote));
    }else if (errno != EAGAIN){
        perror("recvfrom");
    }
    close(udp);
    return 0;
}
