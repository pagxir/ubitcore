#ifndef __KFIND_H__
#define __KFIND_H__
class bdhtnet;
class kfind{
    public:
        kfind(bdhtnet *net, const char target[20]);
        int vcall();

    private:
        char b_target[20];
        int b_concurrency;
        int b_state;
        bdhtnet *b_net;
};
int _find_node(char target[20]);
#endif
