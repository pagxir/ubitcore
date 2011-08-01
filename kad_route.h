#ifndef _KAD_ROUTE_H_
#define _KAD_ROUTE_H_

#define NF_NODE       0x01
#define NF_HELO       0x02
#define NF_ITEM       0x04

#define HASH_LEN 20
#define IDENT_LEN 20
#define TOKEN_LEN 20

struct KAC {
	in_addr kc_addr;
	u_short kc_port;
};

struct kad_node {
	int kn_type;
	KAC kn_addr;
	char kn_ident[IDENT_LEN];
};

int kad_node_seen(struct kad_node *knp);
int kad_node_good(struct kad_node *knp);
int kad_node_timed(struct kad_node *knp);
int kad_node_insert(struct kad_node *knp);

int kad_set_ident(const char *ident);
int kad_get_ident(const char **ident);
int kad_set_token(const char *token);
int kad_get_token(const char **token);

int kad_krpc_closest(const char *target, char *buf, size_t len);
int kad_krpc_closest(const char *target, kad_node *nodes, size_t count);

#endif

