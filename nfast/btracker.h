#ifndef __BTRACKER_H__
#define __BTRACKER_H__
#include <string>

#include "bthread.h"
#include "burlget.h"

class burlthread: public bthread
{
    public:
        burlthread(const char *url, int second);
        virtual int bdocall(time_t timeout);
        ~burlthread();

    private:
        int b_state;
        int b_second;
        int last_time;
        char b_path[512];
        burlget *b_get;
        std::string b_url;
        std::string b_data;
};

#endif
