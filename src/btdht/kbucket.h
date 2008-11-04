#ifndef __KBUCKET_H__
#define __KBUCKET_H__
class knode;
struct kitem_t;
class kbucket{
    public:
        kbucket();
        void touch();
        int  get_knode(kitem_t nodes[_K]);
        int  update_contact(const kitem_t *in, kitem_t *out);

    private:
        int    b_count;
        time_t b_last_seen;
        knode  *b_knodes[_K];
};
int update_boot_contact(in_addr_t addr, in_port_t port);
int update_contact(const kitem_t *in, kitem_t *out);
int get_knode(char target[20], kitem_t nodes[_K], bool valid);
#endif
