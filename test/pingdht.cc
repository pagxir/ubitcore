#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "btcodec.h"

static const char __ping_struct[] = {
    "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t1:P1:y1:qe"
};

const char *idstr(const char *id)
{
    int i;
    static char __idstr[41];
    for (i=0; i<20; i++){
        sprintf(__idstr+2*i, "%02x", id[i]&0xff);
    }
    return __idstr;
}

void ping_dump(const char *buff, size_t len,
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
    return;
}

int main(int argc, char *argv[])
{
    int i;
    int error;
    char buffer[8192];
    struct sockaddr_in remote, sender;
    int udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (argc < 3){
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
    remote.sin_port = htons(atoi(argv[2]));
    remote.sin_addr.s_addr = inet_addr(argv[1]);
    for (i=0; i<3; i++){
        printf("send: %d\n", i);
        error = sendto(udp, __ping_struct, strlen(__ping_struct), 0,
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
        ping_dump(buffer, error, &remote, sizeof(remote));
    }else if (errno != EAGAIN){
        perror("recvfrom");
    }
    close(udp);
    return 0;
}
