#ifndef _UDP_DAEMON_H_
#define _UDP_DAEMON_H_

int kad_setident(const char * ident);
int kad_pingnode(const char * server);
int kad_getpeers(const char * ident, const char * server);
int kad_findnode(const char * ident, const char * server);

#endif

