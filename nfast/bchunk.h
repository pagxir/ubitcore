#ifndef __BCHUNK_H__
#define __BCHUNK_H__

struct bchunk_t{
    int b_index;
    int b_start;
    int b_length;
};

bchunk_t *bchunk_get(int index, unsigned char *bitset, int count);
#endif
