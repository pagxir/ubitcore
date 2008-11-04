#ifndef __KNODE_H__
#define __KNODE_H__
class knode{
    public:
        knode();
        knode(char id[20], in_addr_t addr, in_port_t port);
        void touch();

    private:
        in_addr_t b_address;
        in_port_t b_port;
        bool   b_destroy;
        bool   b_valid;
        char   b_ident[20];
        time_t b_last_seen;
};
#endif
