#ifndef __BOOTUPD_H__
#define __BOOTUPD_H__

class bootupd: public bthread
{
    public:
        bootupd();
        virtual int bdocall();

    private:
        int b_state;
        kfind  *b_find;
        bool   b_refresh;
        bool   b_usevalid;

    private:
        time_t b_random;
        time_t b_start_time;
};

#endif
