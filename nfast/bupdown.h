#ifndef __BUPDOWN_H__
#define __BUPDOWN_H__
#include <memory>
#include "bthread.h"
#include "bpeermgr.h"
#include "bsocket.h"

struct bupload_wrapper;

class bupdown: public bthread
{
    public:
        bupdown();
        virtual int bfailed();
        int bupload(time_t timeout);
        virtual int bdocall(time_t timeout);

    protected:
        int real_decode(char buffer[], int len);
        int quick_decode(char buffer[], int len);

    private:
        ep_t *b_ep;
        int b_state;
        int b_offset;
        char b_buffer[1<<14];
        std::auto_ptr<bupload_wrapper> b_upload;
};
#endif
