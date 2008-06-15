#ifndef __BOUTFILE_H__
#define __BOUTFILE_H__
class boutfile
{
    public:
        virtual int bopen()=0;
        virtual int bpathbind(const char *path)=0;
        virtual int bwrite(const char *buf, int len)=0;
        virtual int bclose()=0;
        virtual ~boutfile()=0;
        static boutfile *get();
};
#endif
