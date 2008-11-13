#ifndef __KBUCKET_H__
#define __KBUCKET_H__
class knode;
struct kitem_t;
class kbucket{
    public:
        kbucket();
        ~kbucket();
        void dump();
        void touch();

        int  failed_contact(const kitem_t *in);
        int  find_nodes(kitem_t nodes[_K], bool validate);
        int  update_contact(const kitem_t *in, bool contacted);

    private:
        time_t b_last_seen;

        int     b_nknode;
        knode  *b_knodes;

        int     b_nbackup;
        knode  *b_backups;
};

#endif
