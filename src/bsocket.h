#ifndef __BSOCKET_H__
#define __BSOCKET_H__
#include <queue>
#include "bthread.h"
#define BSF_QUEUE 0x1000
#define BSF_WRITE 0x2000
#define BSF_READ  0x4000
#define BSF_CONN  0x8000

#define FF_MASK      0x00FF
#define FF_NOCONNECT 0x1000

struct nextfds;

class bsocket
{
    public:
        bsocket();
        ~bsocket();
        bsocket &operator=(bsocket&);
        int baccept(uint32_t *host, int *port);
        int bconnect(const char *host, int port);
        int bconnect(uint32_t host, int port);
        int bsend(const void *buf, size_t len);
        int breceive(void *buf, size_t len);
        int bsendto(const void *buf, size_t len,
                uint32_t host, unsigned short port);
        int brecvfrom(void *buf, size_t len,
                uint32_t *host, unsigned short *port);
        static int global_init();
        static int bselect(time_t timeout);

    private:
        bsocket(bsocket&);
   
    private:
        int bpoll(int count);
        int enqueue();
        int q_read(bthread *job);
        int q_write(bthread *job);
        int ok_read();
        int ok_write();
        int bwait_send();
        int bwait_connect();
        int bwait_receive();
#ifndef NDEBUG
    public:
#endif
        static int is_busy();
        static int __bwait_send(bthread *job, int argc, void *argv);
        static int __bwait_receive(bthread *job, int argc, void *argv);
        static int __bwait_connect(bthread *job, int argc, void *argv);

    private:
        int b_fd;
        int b_flag;
        int f_flag;
#ifndef DEFAULT_TCP_TIME_OUT
        time_t b_syn_time;
#endif
        bthread *b_jwr, *b_jrd;

        static int maxfd;
        static nextfds *b_nextfds;
        static std::queue<bsocket*> bqueue;
};
void bdump_socket_crash_message(const char *buf);
#endif
