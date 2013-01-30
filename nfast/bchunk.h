#ifndef __BCHUNK_H__
#define __BCHUNK_H__
#include <stdio.h>
#include "bitfield.h"

struct bchunk_t
{
    int b_index;
    int b_start;
    int b_length;

    bchunk_t(int idx=0, int off=0, int len=0)
    {
        b_index = idx;
        b_start = off;
        b_length = len;
    }
};

size_t bcount_piece();
size_t bmap_count();
int bend_key();
int bget_have(int idx);
int bcancel_request(int idx);
int bset_piece_info(int length, int count, int rest);
int bchunk_copyto(char *buf, bchunk_t *chunk);
int bchunk_sync(const char *buf, int idx, int start, int len);
int bfile_sync(FILE *fp, int piece, int start);
bchunk_t *bchunk_get(int index, bitfield &bitset,
        int *lidx, int *bendkey, int *lcount, int *bhave);
#endif
