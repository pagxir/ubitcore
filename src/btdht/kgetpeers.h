#ifndef __KFIND_H__
#define __KFIND_H__
#include "btkad.h"
#include "kutils.h"
#include <vector>
class bdhtnet;
class kgetpeers_arg;
struct kitem_t;
class kgetpeers{
    public:
        kgetpeers(bdhtnet *net, const char target[20], kitem_t items[], size_t count);
        int vcall();
        void decode_packet(const char buffer[], size_t count,
                in_addr_t host, in_port_t port, const char kadid[20]);

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
        std::vector<kgetpeers_arg*> b_kgetpeers_out;
        std::map<kaddist_t, kgetpeers_arg*> b_kgetpeers_queue;
        std::map<kaddist_t, int> b_kgetpeers_outed;
        std::map<kaddist_t, int> b_kgetpeers_ined;
};
#endif
