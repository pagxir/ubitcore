#ifndef __KPINGD_H__
#define __KPINGD_H__
#include <map>
#include "kutils.h"

class pingd: public bthread
{
    public:
        pingd();
        ~pingd();
        void dump();
        virtual int bdocall();
        int add_contact(in_addr_t addr, in_port_t port);

    private:
        kship *b_ship;
        bool b_sendmore;
        time_t b_last_seen;
        std::string b_text_satus;
        int kping_expand(char *buffer, int count, in_addr_t addr, in_port_t port);

    private:
        int b_state;
        int b_concurrency;
        std::map<in_addr_t, kitem_t> b_queue;
};
#endif
