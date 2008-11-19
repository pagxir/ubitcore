
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <assert.h>
#include <map>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>

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

int main(int argc, char *argv[])
{
    char __target[20];
    int udp = socket(AF_INET, SOCK_STREAM, 0);

    if (argc < 2){
        fprintf(stderr, "too few argument\n");
        return -1;
    }

    struct sockaddr_in siaddr;
    siaddr.sin_family = AF_INET;
    siaddr.sin_port   = htons(6889);
    siaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    memcpy(__target, strid(argv[1]), 20); 

    int e = connect(udp, (sockaddr*)&siaddr, sizeof(siaddr));
    assert(e != -1);

    e = send(udp, __target, 20, 0);
    assert(e != -1);

    close(udp);
    return 0;
}
