#ifndef _PROT_KAD_H_
#define _PROT_KAD_H_

class btentity;

int kad_ping_node(void *buf, size_t len, uint32_t tid);
int kad_less_than(const char *sp, const char *lp, const char *rp);
int kad_find_node(void *buf, size_t len, uint32_t tid, const uint8_t *ident);
int kad_get_peers(void *buf, size_t len, uint32_t tid, const uint8_t *ident);

int kad_ping_node_answer(void *buf, size_t len, btentity *tp);
int kad_error_node_answer(void *buf, size_t len, btentity *tp);

int kad_find_node_answer(void *buf, size_t len, btentity *tp, const char *inp, size_t inl);
int kad_get_peers_answer(void *buf, size_t len, btentity *tp,
				const char *inp, size_t inl, const char *valp, size_t vall);

#endif

