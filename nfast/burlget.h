#ifndef __BURLGET_H__
#define __BURLGET_H__
class burlget
{
    public:
        static burlget *get();
        virtual int bdopoll(time_t timeout)=0;
        virtual ~burlget();
};
#endif
