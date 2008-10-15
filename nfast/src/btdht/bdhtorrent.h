/* $Id: bdhtnet.cc 94 2008-07-19 04:12:35Z pagxir $ */
#ifndef __BDHTORRENT_H__
#define __BDHTORRENT_H__

class bdhtorrent: public bdhtpoller
{
    public:
        bdhtorrent();
        void get_peers(const void *ibuf, size_t len);
        virtual int bdocall(time_t timeout);

    private:
        int b_index;
        int b_state;
        time_t b_touch;
        std::queue<bootstraper*> b_tryagain;
        std::queue<bootstraper*> b_tryfinal;
        std::map<netpt,bootstraper*> b_trapmap;
        std::map<bdhtident,bootstraper*> b_findmap;
        std::map<bdhtident,bootstraper*> b_bootmap;
};

#endif
