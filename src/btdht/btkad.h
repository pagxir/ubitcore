#ifndef __BTKAD_H__
#define __BTKAD_H__
#define  _K 8
#define CONCURRENT_REQUEST 3
class kfind;
struct kitem_t;

class btkad{
    public:
        static kfind *find_node(char target[20]);
};

struct kaddist_t{
    uint8_t kaddist[20];
    kaddist_t();
    kaddist_t(const uint8_t kadid[20]);
    kaddist_t(const char kadid1[20], const char kadid2[20]);
    bool operator>(const kaddist_t &op2)const;
    bool operator<(const kaddist_t &op2)const;
    bool operator==(const kaddist_t &op2)const;
    kaddist_t &operator=(const kaddist_t &op);
};

struct kadid_t{
    char kadid[20];
    kadid_t();
    kadid_t(const char kadid[20]);
    kaddist_t operator^(const kadid_t &op2)const;
    kadid_t &operator=(const kadid_t &op);
};

int bdhtnet_start();
int find_nodes(const char *target, kitem_t items[_K], bool valid);
int genkadid(char ident[20]);
int getkadid(char ident[20]);
int setkadid(const char ident[20]);
int get_kbucket_index(const char ident[20]);
int add_boot_node(in_addr_t host, in_port_t port);
#endif
