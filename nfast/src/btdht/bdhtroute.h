#ifndef __BDHTROUTE_H__
#define __BDHTROUTE_H__
class bdhtnet;
int getribcount();
void route_get_peers(bdhtnet *net);
void dump_route_table();
void update_route(const void *ibuf, size_t len, uint32_t host, uint16_t port);
#endif
