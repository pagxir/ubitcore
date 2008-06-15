#ifndef __BURLGET_H__
#define __BURLGET_H__
class burlget
{
    public:
        static burlget *get();
        virtual int burlbind(const char *url)=0;
        virtual int bdopoll(time_t timeout)=0;
        virtual ~burlget();
};
#endif
