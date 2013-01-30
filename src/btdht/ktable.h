#ifndef __KTABLE_H__
#define __KTABLE_H__

class knode;
class kbucket;

class ktable{
    public:
        ktable();
        ~ktable();
        void dump();

    public:
        time_t last_seen(){ return b_last_seen; }
        int size(){ return b_nbucket0; }
        int getkadid(char kadid[20]);
        int setkadid(const char kadid[20]);

    public:
        int bit1_index_of(const char target[20]);
        int failed_contact(const kitem_t *in);
        int insert_node(const kitem_t *in, bool contacted);
        int find_nodes(const char target[20], kitem_t items[8], bool validate=true);

    private:
        int b_count0;
        char b_tableid[20];
        time_t b_last_seen;

    private:
        int b_nbucket0;
        int b_nbucket1;
        kbucket *b_buckets;

    public:
        int get_ping(kitem_t *item);
        bool need_ping(){ return b_need_ping; }

    private:
        bool b_need_ping;
};

#endif
