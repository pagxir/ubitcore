#ifndef __KNODE_H__
#define __KNODE_H__

class knode{
    public:
        knode();
        knode(const char id[20], in_addr_t addr, in_port_t port);
        int  touch();
        int  failed();
        bool _isgood();
        bool _isvalidate() { return b_failed<3; }
        int  get(kitem_t *out);
        int  set(const kitem_t *in);
        int  cmpid(const char id[20]);
        time_t last_seen() { return b_last_seen; }
        in_addr_t addr() { return b_address; }
        in_port_t port() { return b_port; }

    private:
        in_addr_t b_address;
        in_port_t b_port;
        int    b_failed;
        char   b_ident[20];
        time_t b_birthday;
        time_t b_last_seen;
};
#endif
