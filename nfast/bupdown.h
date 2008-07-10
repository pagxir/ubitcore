#ifndef __BUPDOWN_H__
#define __BUPDOWN_H__
#include <memory>
#include <vector>
#include "bitfield.h"
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
        int quick_decode(char buffer[], int len, int ql);

    private:
        int b_lchoke;
        int b_new_lchoke;
        int b_linterested;
        int b_new_linterested;

        int b_rchoke;
        int b_rinterested;

        int b_keepalive;
        int b_requesting;

    private:
        int b_ref_have;
        int b_lidx;
        int b_lcount;

    private:
        int b_ptrhave;
        int b_lastref;
        int b_upoff;
        int b_upsize;
        char b_upbuffer[1<<17];

    private:
        ep_t b_ep;
        int b_state;
        int b_offset;
        char b_buffer[1<<17];
        bitfield b_bitfield;
        std::auto_ptr<bupload_wrapper> b_upload;
};

bool bglobal_break();
#endif
