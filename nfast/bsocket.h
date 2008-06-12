#ifndef __BSOCKET_H__
#define __BSOCKET_H__
#include <sys/select.h>
#include <queue>
#define BSF_QUEUE 0x1000
#define BSF_WRITE 0x2000
#define BSF_READ  0x4000

struct nextfds;

class bsocket
{
    public:
        bsocket();
        static int global_init();
        static int bselect(time_t timeout);
   
    private:
        int bpoll(int count);
        int enqueue();
        int q_read();
        int q_write();
        int ok_read();
        int ok_write();
        static int is_busy();

    private:
        int b_fd;
        int b_flag;

        static int maxfd;
        static nextfds *b_nextfds;
        static std::queue<bsocket*> bqueue;
};
#endif
