#ifndef _UDP_DAEMON_H_
#define _UDP_DAEMON_H_

int kad_setident(const char *ident);
int kad_pingnode(const char *server);
int kad_getpeers(const char *ident, const char *server);
int kad_findnode(const char *ident, const char *server);
int kad_proto_out(int type, const char *target,
	   	const struct sockaddr_in *soap, char *outp,
	   	size_t size, struct waitcb *waitcbp);

#endif

