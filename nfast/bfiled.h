#ifndef __BFILED_H__
#define __BFILED_H__
#include <set>
#include "bthread.h"

class bfiled: public bthread
{
    public:
        bfiled(int second);
        virtual int bdocall(time_t timeout);

    private:
        int b_second;
        int last_time;
};

struct bfile_info{
    int b_piece;
    int b_start;
    FILE *b_fd;

    bfile_info(int idx, int off, FILE *fp)
    {
        b_fd = fp;
        b_piece = idx;
        b_start = off;
    }
};

inline bool operator<(bfile_info l, bfile_info r)
{
    return l.b_piece==r.b_piece?
        l.b_start<r.b_start:
        l.b_piece<r.b_piece;
}


int bfile_sync(std::set<bfile_info> &filelist);
int badd_per_file(int piece, int start, const char *path);
int bfiled_start(int time);
#endif
