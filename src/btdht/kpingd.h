#ifndef __KPINGD_H__
#define __KPINGD_H__
#include "kutils.h"

struct kping_t{
    kitem_t    item;
    kship     *ship;
};

class pingd: public bthread
{
    public:
        pingd();
        void dump();
        virtual int bdocall();
        int add_contact(in_addr_t addr, in_port_t port);

    private:
        bool b_sendmore;
        time_t b_last_seen;
        std::string b_text_satus;

    private:
        int b_state;
        int b_concurrency;
        std::vector<kping_t> b_queue;
};
#endif
