#ifndef _KAD_ROUTE_H_
#define _KAD_ROUTE_H_

#define NF_GOOD       0x01
#define NF_UNKOWN     0x02
#define NF_DOUBT      0x04
#define NF_DEAD       0x08

struct kad_node2 {
	int kn_flag;
	char kn_ident[20];
	in_addr kn_addr;
	u_short kn_port;
};

int kad_node_seen(const char *ident, in_addr in_addr1, u_short in_port1);
int kad_node_good(const char *ident, in_addr in_addr1, u_short in_port1);
int kad_node_timed(const char *ident, in_addr in_addr1, u_short in_port1);
int kad_node_insert(const char *ident, in_addr in_addr1, u_short in_port1);
int kad_node_closest(const char *ident, struct kad_node2 *closest, size_t count);

#endif

