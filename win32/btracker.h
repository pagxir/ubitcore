#ifndef __BTRACKER_H__
#define __BTRACKER_H__
#include <string>
#include <memory>

#include "bthread.h"
#include "burlget.h"

class burlthread: public bthread
{
    public:
        burlthread(const char *url, const char *url1st,
				const char *url2nd, int second);
        virtual int bdocall(time_t timeout);
        ~burlthread();

    private:
        int b_1st;
        int b_state;
        int b_second;
        int last_time;
        char b_path[512];
        std::string b_data;
        std::string b_url, b_url1st, b_url2nd;
        std::auto_ptr<burlget> b_get;
};

#endif
