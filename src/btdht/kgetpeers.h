#ifndef __KGETPEERS_H__
#define __KGETPEERS_H__
#include "btkad.h"
#include "kutils.h"
#include <vector>
class kship;
class bdhtnet;

struct kgetpeers_t{
    kitem_t    item;
    kship     *ship;
};

class kgetpeers{
    public:
        kgetpeers(bdhtnet *net, const char target[20],
                kitem_t items[], size_t count);
        void kgetpeers_expand(const char buffer[], size_t count,
                in_addr_t host, in_port_t port, const kitem_t *old);
        int vcall();

    private:
        int b_sumumery;
        int b_last_concurrency;

    private:
        char b_target[20];
        int b_concurrency;
        int b_state;
        bdhtnet *b_net;
        time_t   b_last_update;
        bool      b_trim;
        kaddist_t b_ended;

    private:
        std::vector<kgetpeers_t> b_outqueue;
        std::map<kaddist_t, kgetpeers_t> b_qfind;
        std::map<kaddist_t, int> b_mapoutedkadid;
        std::map<in_addr_t, int> b_mapoutedaddr;
        std::map<kaddist_t, int> b_mapined;
};
#endif
