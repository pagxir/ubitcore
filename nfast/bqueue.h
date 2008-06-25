#ifndef __BQUEUE_H__
#define __BQUEUE_H__
#include <memory>
#include "bthread.h"
#include "bpeermgr.h"
#include "bsocket.h"

class bqueue: public bthread
{
    public:
        bqueue();
        virtual int bfailed();
        virtual int bdocall(time_t timeout);

    private:
        std::auto_ptr<bsocket> b_socket;
        std::string b_protocol;
        int b_state;
        ep_t b_ep;
};
#endif
