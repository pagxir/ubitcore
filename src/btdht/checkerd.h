#ifndef __CHECKERD_H__
#define __CHECKERD_H__

class checkerd: public bthread
{
    public:
        checkerd();
        virtual int bdocall();

    private:
        int b_state;
        time_t b_last_show;
        time_t b_next_refresh;
};

#endif
