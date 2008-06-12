#include <time.h>
#include <stdio.h>
#include <queue>

#include "bthread.h"
#include "bsocket.h"

class biothread: public bthread
{
    public:
        virtual int bdocall(time_t timeout);
};

static std::queue<bsocket*> __b_sockets;

int biothread::bdocall(time_t outtime)
{
    int maxfd = -1;
    timeval tval;
    tval.tv_sec = outtime-time(NULL);
    tval.tv_usec = 0;
    fd_set readfds, writefds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    printf("bselect\n");
    if (__b_sockets.empty()){
	return btime_wait(time(NULL)+5);
    }

    while (__b_sockets.empty()){
        bsocket *bs = __b_sockets.front();
        __b_sockets.pop();
        maxfd = bs->bpoll(maxfd, &readfds, &writefds);
    }

    int count = select(maxfd+1, &readfds, &writefds, NULL, &tval);
    printf("select: %d\n", count);

    bwakeup();
    
    return 0;
}

static biothread __iothread;

int bioinit()
{
    __iothread.bwakeup();
    return 0;
}
