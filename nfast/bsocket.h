#ifndef __BSOCKET_H__
#define __BSOCKET_H__
#include <sys/select.h>
#include <queue>
#include "bthread.h"
#define BSF_QUEUE 0x1000
#define BSF_WRITE 0x2000
#define BSF_READ  0x4000

struct nextfds;

class bsocket
{
    public:
        bsocket();
        int bsend();
        int bconnect();
        static int global_init();
        static int bselect(time_t timeout);
   
    private:
        int bpoll(int count);
        int enqueue();
        int q_read(bthread *job);
        int q_write(bthread *job);
        int ok_read();
        int ok_write();
        int bwait_connect();
        static int is_busy();
        static int __bwait_connect(bthread *job, int argc, void *argv);

    private:
        int b_fd;
        int b_flag;
        bthread *b_jwr, *b_jrd;

        static int maxfd;
        static nextfds *b_nextfds;
        static std::queue<bsocket*> bqueue;
};
#endif
