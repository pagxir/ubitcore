/* $Id$ */
#include <time.h>
#include <string.h>
#include <assert.h>
#include <memory>
#include <set>

#include "bfiled.h"
#include "bupdown.h"
#include "bchunk.h"

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
        bfile_sync(__qfile_list);
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
