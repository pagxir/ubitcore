#ifndef __BTRACKER_H__
#define __BTRACKER_H__
#include <string>
#include <memory>

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
        std::string b_url;
        std::string b_data;
        std::auto_ptr<burlget> b_get;
};

#endif
