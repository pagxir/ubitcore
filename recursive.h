#ifndef _RECURSIVE_H_
#define _RECURSIVE_H_

#define RC_TYPE_PING_NODE 0
#define RC_TYPE_FIND_NODE 1
#define RC_TYPE_GET_PEERS 2

const int MAX_SEND_OUT = 3;
const int MAX_PEER_COUNT = 9;

struct recursive_node: public kad_node
{
	int rn_type;
	int rn_nout;
	int rn_access;
};

struct recursive_context {
	int rc_tid;
	int rc_type;
	int rc_acked;
	int rc_total;
	int rc_touch;
	int rc_sentout;

	char rc_target[20];
	struct waitcb rc_timeout;
	struct kad_node rc_well_nodes[8];
	struct recursive_node rc_nodes[MAX_PEER_COUNT];

	struct recursive_context *rc_next;
	struct recursive_context **rc_prev;
};

int kad_recursive(int type, const char *ident);
int kad_search_update(int tid, const char *ident, btcodec *btcodec);

#endif

