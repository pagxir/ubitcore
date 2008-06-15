#include <stdio.h>
#include "boutfile.h"

class bdistoutfile: public boutfile
{
    public:
        bdistoutfile();
        virtual int bpathbind(const char *path);
        virtual int bopen();
        virtual int bwrite(const char *buf, int len);
        virtual int bclose();
        ~bdistoutfile();
};

boutfile *boutfile::get()
{
    return new bdistoutfile();
}

boutfile::~boutfile()
{
}

bdistoutfile::bdistoutfile()
{
}

int bdistoutfile::bpathbind(const char *path)
{
    printf("bind path: %s\n", path);
    return 0;
}

int bdistoutfile::bopen()
{
    return 0;
}

int bdistoutfile::bwrite(const char *buf, int len)
{
    return 0;
}

int bdistoutfile::bclose()
{
    return 0;
}

bdistoutfile::~bdistoutfile()
{
}
