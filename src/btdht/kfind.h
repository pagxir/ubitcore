#ifndef __KFIND_H__
#define __KFIND_H__
#include "btkad.h"
#include "kutils.h"
#include <vector>
#include <string>
class kship;
class bdhtnet;

class kfind{
    public:
        kfind(bdhtnet *net, const char target[20],
                kitem_t items[], size_t count);
        ~kfind();
        void dump();
        void kfind_expand(const char buffer[],
                size_t count, in_addr_t host, in_port_t port);
        int vcall();

    private:
        int b_sumumery;
        int b_last_finding;
        std::string b_loging;

    private:
        char b_target[20];
        int b_concurrency;
        int b_state;

    private:
        kaddist_t b_ended;
        kship    *b_ship;
        bool      b_trim;
        time_t    b_last_update;

    private:
        std::map<kaddist_t, kitem_t> b_qfind;
        std::map<kaddist_t, kitem_t> b_outqueue;
        std::map<kaddist_t, int> b_mapoutedkadid;
        std::map<in_addr_t, int> b_mapoutedaddr;
        std::map<kaddist_t, int> b_mapined;
};
#endif
