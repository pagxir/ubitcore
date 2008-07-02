/* $Id$ */
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "bthread.h"
#include "biothread.h"
#include "bsocket.h"

#ifndef INADDR_NONE
#define INADDR_NONE -1
#endif

/*
 * NOTICE: If connection not finish, donot minitor read or write.
 * In this case, It would be not work correctly now!
 * NOTICE: Is using FD_ZERO better than using FD_CLR?
 */

struct nextfds
{
    struct nextfds *next;
    fd_set readfds, writefds;
};

static std::queue<bthread*> __q_conflict;
class conflict: public bthread
{
    public:
        conflict();
        virtual int bdocall(time_t timeout);
};

conflict::conflict()
{
    b_ident = "conflict";
}

int
conflict::bdocall(time_t timeout)
{
    while(!__q_conflict.empty()){
        __q_conflict.front()->bwakeup();
        __q_conflict.pop();
    }
    return 0;
}

static conflict __conflict;

int
bsocket::bwait_connect()
{
    if (errno == EINPROGRESS){
	   	bthread_waiter(bsocket::__bwait_connect, 0, this);
    }
    return 0;
}

int
bsocket::bwait_receive()
{
    if (errno == EAGAIN){
        bthread_waiter(bsocket::__bwait_receive, 0, this);
    }else{
        perror("bad bwait_receive");
    }
    return 0;
}

int
bsocket::bwait_send()
{
    if (errno == EAGAIN){
        bthread_waiter(bsocket::__bwait_send, 0, this);
    }else{
        perror("bad bwait_send");
    }
    return 0;
}

bsocket::bsocket()
{
    b_fd = -1;
    b_flag = 0;
    f_flag = 0;
    b_jwr = NULL;
    b_jrd = NULL;
}

int
bsocket::baccept(unsigned long *host, int *port)
{
    return 0;
}

int
bsocket::bsend(const char *buf, size_t len)
{
    int error = send(b_fd, buf, len, 0);
    if (error == -1){
        bwait_send();
    }
    return error;
}

int
bsocket::breceive(char *buf, size_t len)
{
    int error = recv(b_fd, buf, len, 0);
    if (error == -1){
        bwait_receive();
    }
    return error;
}

int
bsocket::ok_read()
{
    assert(b_fd != -1);
    nextfds *resultfds = b_nextfds->next;
    return FD_ISSET(b_fd, &resultfds->readfds);
}

int
bsocket::ok_write()
{
    assert(b_fd != -1);
    nextfds *resultfds = b_nextfds->next;
    return FD_ISSET(b_fd, &resultfds->writefds);
}

int
bsocket::bpoll(int count)
{
    int flag = b_flag;
    b_flag = 0;
    if (BSF_CONN&flag){
        if (count>0 && ok_read()){
            b_jrd->bwakeup();
            flag &= ~BSF_CONN;
            count--;
        }
        if (count>0 && ok_write()){
            b_jwr->bwakeup();
            flag &= ~BSF_CONN;
            count--;
        }
#ifndef DEFAULT_TCP_TIME_OUT
        if (b_syn_time+8 < time(NULL)){
            b_jwr->bwakeup();
            b_jrd->bwakeup();
            flag &= ~BSF_CONN;
        }
#endif
        if (flag & BSF_CONN){
            q_read(b_jrd);
            q_write(b_jwr);
            b_flag |= BSF_CONN;
        }else{
            b_jwr = NULL;
            b_jrd = NULL;
        }
        return count;
    }
    if (BSF_READ&flag){
        if (count>0 && ok_read()){
            b_jrd->bwakeup();
            b_jrd = NULL;
            count --;
        }else{
            q_read(b_jrd);
        }
    }
    if (BSF_WRITE&flag){
        if (count>0 && ok_write()){
            b_jwr->bwakeup();
            b_jwr = NULL;
            count --;
        }else{
            q_write(b_jwr);
        }
    }
    return count;
}

int
bsocket::q_write(bthread *job)
{
    if (b_jwr == NULL){
        b_jwr = job;
    }
    if (job!=b_jwr){
        if (b_jwr != &__conflict){
            __q_conflict.push(b_jwr);
        }
        if (job != &__conflict){
            __q_conflict.push(job);
        }
        b_jwr = &__conflict;
    }
    if (b_flag & BSF_WRITE){
        return 0;
    }
    FD_SET(b_fd, &b_nextfds->writefds);
    b_flag |= BSF_WRITE;
    enqueue();
    return 0;
}

int
bsocket::q_read(bthread *job)
{
    if (b_jrd == NULL){
        b_jrd = job;
    }
    if (job!=b_jrd){
        if (b_jrd != &__conflict){
            __q_conflict.push(b_jrd);
        }
        if (job != &__conflict){
            __q_conflict.push(job);
        }
        b_jrd = &__conflict;
    }
    if (b_flag & BSF_READ){
        return 0;
    }
    FD_SET(b_fd, &b_nextfds->readfds);
    b_flag |= BSF_READ;
    enqueue();
    return 0;
}

int
bsocket::enqueue()
{
    if (b_flag & BSF_QUEUE){
        return 0;
    }
    maxfd = std::max(maxfd, b_fd);
    b_flag |= BSF_QUEUE;
    bqueue.push(this);
    biorun();
    return 0;
}

int
bsocket::bselect(time_t outtime)
{
    int count;
    timeval tval;
    time_t now = time(NULL);
    tval.tv_sec = outtime>now?outtime-now:0;
    tval.tv_usec = 0;
    bsocket marker;

    if (is_busy()){
        count = select(maxfd+1, &b_nextfds->readfds,
                &b_nextfds->writefds, NULL, outtime==-1?NULL:&tval);
        assert(count != -1);
        b_nextfds = b_nextfds->next;
        FD_ZERO(&b_nextfds->readfds);
        FD_ZERO(&b_nextfds->writefds);
        bqueue.push(&marker);
        bsocket *header = bqueue.front();
        bqueue.pop();
        while(header != &marker){
            count = header->bpoll(count);
            header = bqueue.front();
            bqueue.pop();
        }
    }
    return 0;
}

int
bsocket::bconnect(const char *host, int port)
{
    unsigned long rhost = inet_addr(host);
    if (inet_addr(host) == INADDR_NONE){
        hostent *phost = gethostbyname(host);
        if (phost == NULL){
            return -1;
        }
        rhost = *(u_long*)phost->h_addr;
        fprintf(stderr, "IP: %s\n", inet_ntoa(*(in_addr*)&rhost));
    }
    return bconnect(rhost, port);
}

int
bsocket::bconnect(unsigned long host, int port)
{
    int error;
    int fflag;
    sockaddr_in siaddr;
    if (f_flag & FF_NOCONNECT){
        f_flag &= ~FF_NOCONNECT;
#ifndef DEFAULT_TCP_TIME_OUT
        if (b_syn_time+8 < time(NULL)){
			return -1;
		}
#endif
        return (f_flag&FF_MASK);
    }
    assert(b_fd == -1);
    siaddr.sin_family = AF_INET;
    siaddr.sin_port = htons(port);
    siaddr.sin_addr.s_addr = host;
    b_fd = socket(AF_INET, SOCK_STREAM, 0);
    fflag = fcntl(b_fd, F_GETFL);
    fflag |= O_NONBLOCK;
    fcntl(b_fd, F_SETFL, fflag);
    error = connect(b_fd, (sockaddr*)&siaddr, sizeof(siaddr));
    if (error==-1){
        f_flag |= FF_NOCONNECT;
#ifndef DEFAULT_TCP_TIME_OUT
        b_syn_time = time(NULL);
#endif
        bwait_connect();
    }
    return error;
}

int
bsocket::bsendto(const char* buffer, size_t len,
        unsigned long host, unsigned short port)
{
    int fflag;
    sockaddr_in siaddr;
    if (b_fd == -1){
        b_fd = socket(AF_INET, SOCK_DGRAM, 0);
        fflag = fcntl(b_fd, F_GETFL);
        fflag |= O_NONBLOCK;
        fcntl(b_fd, F_SETFL, fflag);
    }
    siaddr.sin_family = AF_INET;
    siaddr.sin_port = htons(port);
    siaddr.sin_addr.s_addr = host;
    return sendto(b_fd, buffer, len, 0, (sockaddr*)&siaddr, sizeof(siaddr));
}

int
bsocket::brecvfrom(char* buffer, size_t len,
        unsigned long *host, unsigned short *port)
{
    assert(b_fd != -1);
    sockaddr_in siaddr;
    socklen_t silen = sizeof(siaddr);
    int error = recvfrom(b_fd, buffer, len, 0, (sockaddr*)&siaddr, &silen);
    if (error == -1){
        bwait_receive();
        return -1;
    }
    if (host != NULL){
        *host = siaddr.sin_addr.s_addr;
    }
    if (port != NULL){
        *port = htons(siaddr.sin_port);
    }
    return error;
}

int
bsocket::__bwait_connect(bthread* job, int argc, void *argv)
{
    bsocket *bps = (bsocket*)argv;
    bps->b_flag |= BSF_CONN;
    bps->q_read(job);
    bps->q_write(job);
#if 0
    printf("__bwait_connect: wait connect: %p!\n", job);
#endif
    return 0;
}

int
bsocket::__bwait_receive(bthread* job, int argc, void *argv)
{
    bsocket *bps = (bsocket*)argv;
    bps->q_read(job);
    return 0;
}

int
bsocket::__bwait_send(bthread* job, int argc, void *argv)
{
    bsocket *bps = (bsocket*)argv;
    bps->q_write(job);
    return 0;
}

int
bsocket::is_busy()
{
    return !bqueue.empty();
}

bsocket::~bsocket()
{
    if (b_fd != -1){
        close(b_fd);
        if (b_jrd || b_jwr){
            printf("%p %p %p\n", this, b_jrd, b_jwr);
        }
        assert(b_jwr == NULL);
        assert(b_jrd == NULL);
        FD_CLR(b_fd, &b_nextfds->readfds);
        FD_CLR(b_fd, &b_nextfds->writefds);
        b_fd = -1;
    }
}

int bsocket::maxfd = -1;
std::queue<bsocket*> bsocket::bqueue;
static nextfds nextfds1, nextfds2;
nextfds *bsocket::b_nextfds = &nextfds1;

int
bsocket::global_init()
{
    FD_ZERO(&nextfds2.readfds);
    FD_ZERO(&nextfds1.writefds);
    FD_ZERO(&nextfds1.readfds);
    FD_ZERO(&nextfds2.writefds);
    nextfds2.next = &nextfds1;
    nextfds1.next = &nextfds2;
    return 0;
}

int
bsocket::bshutdown()
{
    assert(b_fd != -1);
    shutdown(b_fd, SHUT_RDWR);
    if (b_jrd != NULL){
        return -1;
    }
    return b_jwr==NULL?0:-1;
}
