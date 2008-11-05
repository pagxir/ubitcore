#ifndef __KFIND_H__
#define __KFIND_H__
#include "btkad.h"
#include <vector>
class bdhtnet;
class kfind_arg;
class kfind{
    public:
        kfind(bdhtnet *net, const char target[20]);
        int vcall();
        void decode_packet(const char buffer[], size_t count,
                in_addr_t host, in_port_t port);

    private:
        char b_target[20];
        int b_concurrency;
        int b_state;
        bdhtnet *b_net;
        time_t   b_last_update;
        bool      b_trim;
        kaddist_t b_ended;
        std::vector<kfind_arg*> b_kfind_out;
        std::map<kaddist_t, kfind_arg*> b_kfind_queue;
        std::map<kaddist_t, int> b_kfind_outed;
        std::map<kaddist_t, int> b_kfind_ined;
};
int _find_node(char target[20]);
#endif
