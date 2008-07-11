#ifndef __BURLGET_H__
#define __BURLGET_H__
#include "boutfile.h"
class burlget
{
    public:
        static burlget *get();
        virtual const char *blocation() = 0;
        virtual int burlbind(const char *url)=0;
        virtual int bpolldata(char *buf, int len)=0;
        virtual ~burlget();
};
#endif
