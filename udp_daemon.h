#ifndef _UDP_DAEMON_H_
#define _UDP_DAEMON_H_

int kad_setident(const char *ident);
int kad_getpeers(const char *ident);
int kad_findnode(const char *ident);

int kad_bootup(const char *peer);
int kad_pingnode(const char *peer);
int send_node_ping(struct kad_node *knp);

int kad_proto_out(int type, const char *target,
	   	const struct sockaddr_in *soap, char *outp,
	   	size_t size, struct waitcb *waitcbp);

#endif

