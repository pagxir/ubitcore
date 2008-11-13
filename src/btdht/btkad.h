#ifndef __BTKAD_H__
#define __BTKAD_H__
#define  _K 8
#define CONCURRENT_REQUEST 3
class kfind;
struct kitem_t;

kfind *kfind_new(char target[20], kitem_t items[], size_t count);

int bdhtnet_start();
int find_nodes(const char *target, kitem_t items[_K], bool valid);
int genkadid(char ident[20]);
int getkadid(char ident[20]);
int setkadid(const char ident[20]);
int add_boot_contact(in_addr_t addr, in_port_t port);
int update_contact(const kitem_t *in, kitem_t *out, bool contacted);
int failed_contact(const kitem_t *in);
int get_knode(char target[20], kitem_t nodes[_K], bool valid);
int refresh_routing_table();
int get_table_size();
int size_of_bucket(int index);
int bit1_index_of(const char kadid[20]);
void dump_routing_table();
#endif
