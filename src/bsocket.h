#ifndef __BSOCKET_H__
#define __BSOCKET_H__
#include <queue>
#include <netinet/in.h>
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
        int baccept(in_addr_t *host, in_port_t *port);
        int bconnect(const char *host, in_port_t port);
        int bconnect(in_addr_t host, in_port_t port);
        int bsend(const void *buf, size_t len);
        int breceive(void *buf, size_t len);
        int bsendto(const void *buf, size_t len,
                in_addr_t host, in_port_t port);
        int brecvfrom(void *buf, size_t len,
                in_addr_t *host, in_port_t *port);
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
