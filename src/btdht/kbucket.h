#ifndef __KBUCKET_H__
#define __KBUCKET_H__
class knode;
struct kitem_t;
class kbucket{
    public:
        kbucket();
        void dump();
        void touch();
        int  failed_contact(const kitem_t *in);
        int  get_knode(kitem_t nodes[_K]);
        int  find_nodes(kitem_t nodes[_K]);
        int  update_contact(const kitem_t *in, kitem_t *out);

    private:
        int    b_count;
        time_t b_last_seen;
        knode  *b_knodes[_K];
};

#endif
