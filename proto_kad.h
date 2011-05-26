#ifndef _PROT_KAD_H_
#define _PROT_KAD_H_

int kad_ping_node(void *buf, size_t len, uint32_t tid);
int kad_find_node(void *buf, size_t len, uint32_t tid, const uint8_t *ident);
int kad_get_peers(void *buf, size_t len, uint32_t tid, const uint8_t *ident);
int kad_set_ident(const uint8_t *ident);

#endif

