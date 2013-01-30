#ifndef __BTKAD_H__
#define __BTKAD_H__
#define  _K 8
#define CONCURRENT_REQUEST 3
class kfind;
struct kitem_t;

kfind *kfind_new(char target[20], kitem_t items[], size_t count);

int bdhtnet_start();
int refresh_routing_table();
void dump_routing_table();
int genkadid(char ident[20]);
int getkadid(char ident[20]);
int setkadid(const char ident[20]);
int find_nodes(const char *target, kitem_t items[_K], bool valid);
int add_boot_contact(in_addr_t addr, in_port_t port);
int update_contact(const kitem_t *in, bool contacted);
int failed_contact(const kitem_t *in);
int bit1_index_of(const char kadid[20]);
int size_of_bucket(int index);
int size_of_table();
#endif
