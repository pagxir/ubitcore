#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
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
        char *b_path;
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
    b_path = NULL;
}

int bdistoutfile::bpathbind(const char *path)
{
    if (b_path != NULL){
        free(b_path);
    }
    b_path = strdup(path);
    return 0;
}

int bdistoutfile::bopen()
{
    assert(b_path != NULL);
    if (b_file == NULL){
        b_file = fopen(b_path, "wb");
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
    if (b_path != NULL){
        free(b_path);
        b_path = NULL;
    }
    bclose();
}
