#ifndef _KAD_ROUTE_H_
#define _KAD_ROUTE_H_

int kad_node_seen(const char *ident, in_addr in_addr1, u_short in_port1);
int kad_node_good(const char *ident, in_addr in_addr1, u_short in_port1);
int kad_node_insert(const char *ident, in_addr in_addr1, u_short in_port1);
int kad_node_failure(const char *ident, in_addr in_addr1, u_short in_port1);

#endif

