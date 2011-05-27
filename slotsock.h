#ifndef _SLOTSOCK_H_
#define _SLOTSOCK_H_
struct sockcb;

struct sockcb *sock_attach(int sockfd);
int sock_detach(struct sockcb *sockcbp);

int sock_wakeup(int sockfd, int index, int event);
int sock_selscan(void *upp);

int sock_read_wait(struct sockcb *sockcbp, struct waitcb *waitcbp, int *statp);
int sock_write_wait(struct sockcb *sockcbp, struct waitcb *waitcbp, int *statp);
int getaddrbyname(const char *name, struct sockaddr_in *addr);
int sock_set_reg_ptr(int (* reg_ptr)(int, int, int));

#endif

