#ifndef __BURLGET_H__
#define __BURLGET_H__
#include "boutfile.h"
class burlget
{
    public:
        static burlget *get();
        virtual int boutbind(boutfile *bfile)=0;
        virtual int burlbind(const char *url)=0;
        virtual int bdopoll(time_t timeout)=0;
        virtual ~burlget();
};
#endif
