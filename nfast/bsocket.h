#ifndef __BSOCKET_H__
#define __BSOCKET_H__
#include <sys/select.h>

class bsocket
{
    public:
        int bpoll(int maxfd, fd_set *readfds, fd_set *writefds);
};
int bioinit();
#endif
