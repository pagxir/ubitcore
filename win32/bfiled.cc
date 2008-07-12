/* $Id$ */
#include <time.h>
#include <string.h>
#include <assert.h>
#include <memory>
#include <set>

#include "bfiled.h"
#include "bupdown.h"
#include "bchunk.h"

static int __fc=0;
static char __text[20];

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

bool operator<(bfile_info l, bfile_info r)
{
    return l.b_piece==r.b_piece?
        l.b_start>r.b_start:
        l.b_piece>r.b_piece;
}

static std::set<bfile_info> __qfile_list;

int
badd_per_file(int piece, int start, const char *path)
{
    FILE *fp= fopen(path, "rb+");
    if (fp == NULL){
        fp = fopen(path, "wb+");
    }
    assert(__qfile_list.find(bfile_info(piece, start, fp))
            == __qfile_list.end());
    __qfile_list.insert(bfile_info(piece, start, fp));
    return 0;
}

bfiled::bfiled(int second)
{
    b_ident = "bfiled";
    b_second = second;
    last_time = time(NULL);
}

int
bfiled::bdocall(time_t timeout)
{
    time_t now;
    time(&now);
#if 0
	/* Should I use merge sort by now ? */
#endif
    while(-1 != btime_wait(last_time+b_second)){
        last_time = time(NULL);
        bfile_sync(NULL, 0, 0);
        for (std::set<bfile_info>::iterator pfile = __qfile_list.begin();
                pfile != __qfile_list.end(); pfile++){
            if (bfile_sync(pfile->b_fd, pfile->b_piece, pfile->b_start)){
                break;
            }
        }
        bglobal_break();
    }
    return -1;
}

static bfiled __bfiled(10);

int
bfiled_start(int time)
{
    __bfiled.bwakeup();
    return 0;
}
