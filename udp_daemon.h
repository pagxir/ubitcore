#ifndef _UDP_DAEMON_H_
#define _UDP_DAEMON_H_

int kad_setup(const char *ident);
int kad_getpeers(const char *ident);
int kad_findnode(const char *ident);

int kad_pingnode(const char *peer);
int send_node_ping(struct kad_node *knp);
int send_bucket_update(const char *node);

int kad_proto_out(int tid, int type, const char *target, const struct sockaddr_in *soap);

#endif

