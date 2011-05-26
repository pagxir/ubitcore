#ifndef _SLOTSOCK_H_
#define _SLOTSOCK_H_
void sock_selscan(void *upp);
void sock_created(int sockfd);
void sock_closed(int sockfd);
void sock_wakeup(int sockfd, int index, int event);
int sock_read_wait(int sockfd, struct waitcb *waitcbp);
int sock_write_wait(int sockfd, struct waitcb *waitcbp);
int getaddrbyname(const char *name, struct sockaddr_in *addr);
int sock_set_reg_ptr(int (* reg_ptr)(int, int, int));

int sendto2(int sockfd, void *buf, size_t len,
		const struct sockaddr_in *so_addr, struct waitcb *waitcbp);

int recvfrom2(int sockfd, void *buf, size_t len,
		struct sockaddr_in *so_addr, struct waitcb *waitcbp);

#endif

