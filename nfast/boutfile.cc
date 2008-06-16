#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
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

    private:
        FILE *b_file;
        std::string b_path;
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
    b_file = NULL;
}

int bdistoutfile::bpathbind(const char *path)
{
    b_path = path;
    return 0;
}

int bdistoutfile::bopen()
{
    assert(!b_path.empty());
    if (b_file == NULL){
        b_file = fopen(b_path.c_str(), "wb");
    }
    return 0;
}

int bdistoutfile::bwrite(const char *buf, int len)
{
    assert(b_file != NULL);
    return fwrite(buf, len, 1, b_file);
}

int bdistoutfile::bclose()
{
    if (b_file != NULL){
        fclose(b_file);
        b_file = NULL;
    }
    return 0;
}

bdistoutfile::~bdistoutfile()
{
    bclose();
}
