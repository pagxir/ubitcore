#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "bthread.h"
#include "biothread.h"
#include "bsocket.h"

struct nextfds
{
    struct nextfds *next;
    fd_set readfds, writefds;
};

static std::queue<bthread*> __q_conflict;
class conflict: public bthread
{
    public:
        virtual int bdocall(time_t timeout);
};

int conflict::bdocall(time_t timeout)
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

bsocket::bsocket()
{
    b_fd = -1;
    b_flag = 0;
    b_jwr = NULL;
    b_jrd = NULL;
}

int
bsocket::bsend()
{
    return 0;
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
    if (BSF_READ&flag){
        if (count>0 && ok_read()){
            assert(b_jrd != NULL);
            b_jrd->bwakeup();
            b_jrd = NULL;
            count --;
        }else{
            q_read(b_jrd);
        }
    }
    if (BSF_WRITE&flag){
        if (count>0 && ok_write()){
            assert(b_jwr != NULL);
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
    tval.tv_sec = outtime-time(NULL);
    tval.tv_usec = 0;
    bsocket marker;

    if (is_busy()){
        count = select(maxfd+1, &b_nextfds->readfds,
                &b_nextfds->writefds, NULL, &tval);
        printf("CC: %d\n", count);
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
bsocket::bconnect()
{
    int error;
    int fflag;
    const char host[] = "www.baidu.com";
    sockaddr_in siaddr;
    assert(b_fd == -1);
    siaddr.sin_family = AF_INET;
    siaddr.sin_port = htons(80);
    siaddr.sin_addr.s_addr = inet_addr(host);
    if (inet_addr(host) == INADDR_NONE){
        hostent *phost = gethostbyname(host);
        if (phost == NULL){
            return -1;
        }
        siaddr.sin_addr.s_addr = *(u_long*)phost->h_addr;
        fprintf(stderr, "IP: %s\n", inet_ntoa(siaddr.sin_addr));
    }
    b_fd = socket(AF_INET, SOCK_STREAM, 0);
#if 1
    fflag = fcntl(b_fd, F_GETFL);
    fflag |= O_NONBLOCK;
    fcntl(b_fd, F_SETFL, fflag);
#endif
    error = connect(b_fd, (sockaddr*)&siaddr, sizeof(siaddr));
    if (error==-1){
        bwait_connect();
    }
    return error;
}

int
bsocket::__bwait_connect(bthread* job, int argc, void *argv)
{
    bsocket *bps = (bsocket*)argv;
    bps->q_read(job);
    bps->q_write(job);
    printf("__bwait_connect: wait connect: %p!\n", job);
    return 0;
}

int
bsocket::is_busy()
{
    return !bqueue.empty();
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
