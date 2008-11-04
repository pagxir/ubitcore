#ifndef __KBUCKET_H__
#define __KBUCKET_H__
class knode;
class kbucket{
    public:
        kbucket();
        void touch();
        int  get_knode(knode nodes[_K]);

    private:
        int    b_count;
        time_t b_last_seen;
        knode  *b_knodes[_K];
};
int get_knode(char target[20], knode nodes[_K], bool valid);
#endif
