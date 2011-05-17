#ifndef _PROT_KAD_H_
#define _PROT_KAD_H_

int kad_get_peers(void * buf, size_t len, uint32_t tid, uint8_t *ident);
int kad_find_node(void * buf, size_t len, uint32_t tid, uint8_t *ident);
int kad_ping_node(void * buf, size_t len, uint32_t tid);

#endif

