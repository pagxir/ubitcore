#ifndef __BDHTROUTE_H__
#define __BDHTROUTE_H__
class bdhtnet;
class bdhtboot;
int getribcount();
void route_get_peers(bdhtnet *net);
void dump_route_table();
void update_route(bdhtnet *net, const void *ibuf, size_t len,
        uint32_t host, uint16_t port);
bool route_need_update(int index);
int  update_boot_nodes(int tabidx, uint32_t host[], uint16_t port[],
        uint8_t idents[][20], size_t size);
int  update_all_bucket(bdhtnet *net);
#endif
