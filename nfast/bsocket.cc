#include <time.h>
#include <assert.h>
#include "biothread.h"
#include "bsocket.h"

struct nextfds
{
    struct nextfds *next;
    fd_set readfds, writefds;
};

bsocket::bsocket()
{
    b_fd = -1;
    b_flag = 0;
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
            count --;
        }else{
            q_read();
        }
    }
    if (BSF_WRITE&flag){
        if (count>0 && ok_write()){
            count --;
        }else{
            q_write();
        }
    }
    return count;
}

int
bsocket::q_write()
{
    if (b_flag & BSF_WRITE){
        return 0;
    }
    FD_SET(b_fd, &b_nextfds->writefds);
    b_flag |= BSF_WRITE;
    enqueue();
    return 0;
}

int
bsocket::q_read()
{
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
