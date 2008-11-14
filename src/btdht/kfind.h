#ifndef __KFIND_H__
#define __KFIND_H__
#include "btkad.h"
#include "kutils.h"
#include <vector>
class kship;
class bdhtnet;

struct kfind_t{
    kitem_t    item;
    kship     *ship;
};

class kfind{
    public:
        kfind(bdhtnet *net, const char target[20],
                kitem_t items[], size_t count);
        int vcall();
        void kfind_expand(const char buffer[], size_t count,
                in_addr_t host, in_port_t port, const kitem_t *old);

    private:
        int b_sumumery;

    private:
        char b_target[20];
        int b_concurrency;
        int b_state;
        bdhtnet *b_net;
        time_t   b_last_update;
        bool      b_trim;
        kaddist_t b_ended;
        std::vector<kfind_t> b_outqueue;
        std::map<kaddist_t, kfind_t> b_qfind;
        std::map<kaddist_t, int> b_mapouted;
        std::map<kaddist_t, int> b_mapined;
};
#endif
