#ifndef __BTKAD_H__
#define __BTKAD_H__
#define  _K 8
#define CONCURRENT_REQUEST _K
class kfind;
class btkad{
    public:
        static kfind *find_node(char target[20]);
};
int bdhtnet_start();
int genkadid(char ident[20]);
int getkadid(char ident[20]);
int setkadid(const char ident[20]);
int get_kbucket_index(const char ident[20]);
int add_knode(char id[20], in_addr_t host, in_port_t port);
#endif
