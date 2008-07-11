/* $Id$ */
#include <time.h>
#include <string.h>
#include <assert.h>
#include <memory>
#include <set>

#include "bfiled.h"
#include "bchunk.h"

struct bfile_info{
    int b_piece;
    int b_start;
    FILE *b_fd;

    bfile_info(int idx, int off)
    {
        b_fd = NULL;
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
badd_per_file(int piece, int start)
{
    assert(__qfile_list.find(bfile_info(piece, start))
            == __qfile_list.end());
    __qfile_list.insert(bfile_info(piece, start));
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
    printf("bcall(%s): %s\n", ident_text.c_str(), ctime(&now));
#endif
    while(-1 != btime_wait(last_time+b_second)){
        last_time = time(NULL);
        bfile_sync(NULL, 0, 0);
        for (std::set<bfile_info>::iterator pfile = __qfile_list.begin();
                pfile != __qfile_list.end(); pfile++){
            printf("sync_file: %d %d\n", pfile->b_piece, pfile->b_start);
            bfile_sync(pfile->b_fd, pfile->b_piece, pfile->b_start);
        }
    }
    return -1;
}

static std::auto_ptr<bfiled> __bfiled;

int
bfiled_start(int time)
{
    __bfiled.reset(new bfiled(time));
    __bfiled->bwakeup();
    return 0;
}
