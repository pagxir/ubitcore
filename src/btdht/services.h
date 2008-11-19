#ifndef __SERVICESD__
#define __SERVICESD__

class kgetpeers;

class serviced: public bthread
{
    public:
        serviced();
        virtual int bdocall();

    private:
        int b_state;
        kgetpeers *b_getpeers;
        bool   b_refresh;
        bool   b_usevalid;

    private:
        bsocket b_socket;
        time_t b_random;
        time_t b_start_time;
};

#endif
