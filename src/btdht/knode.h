#ifndef __KNODE_H__
#define __KNODE_H__

struct kitem_t{
    char kadid[20];
    in_addr_t host;
    in_port_t port;
    time_t    age;
    time_t    atime;
    int       failed;
};

class knode{
    public:
        knode();
        knode(const char id[20], in_addr_t addr, in_port_t port);
        void touch();
        int  XOR(char target[20]);
        bool validate() { return b_valid; }
        int  getnode(kitem_t *out);
        int  set(const kitem_t *in);
        int  replace(const kitem_t *in, kitem_t *out);
        int  failed_contact(const kitem_t *in);
        int  cmpid(const char id[20]);
        int  cmphost(in_addr_t host);
        int  cmpport(in_port_t port);

    private:
        in_addr_t b_address;
        in_port_t b_port;
        int    b_failed;
        bool   b_destroy;
        bool   b_valid;
        char   b_ident[20];
        time_t b_last_seen;
};
#endif
