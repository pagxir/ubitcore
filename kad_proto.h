#ifndef _PROT_KAD_H_
#define _PROT_KAD_H_

class btentity;

int kad_ping_node(void *buf, size_t len, uint32_t tid);
int kad_find_node(void *buf, size_t len, uint32_t tid, const uint8_t *ident);
int kad_get_peers(void *buf, size_t len, uint32_t tid, const uint8_t *ident);
int kad_set_ident(const uint8_t *ident);
int kad_get_ident(char *ident);

int kad_less_than(const char *sp, const char *lp, const char *rp);
int kad_ping_node_answer(void *buf, size_t len, btentity *entity);
int kad_find_node_answer(void *buf, size_t len, btentity *entity, const char *inp, size_t inl);

#endif

