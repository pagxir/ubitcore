/* $Id:$ */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <set>

#include "bchunk.h"
#define BLOCK_SIZE (16*1024)

static std::vector<unsigned char> __q_visited;

struct bmgrchunk
{
    int b_index;
    int b_started;
    int b_length;

    bool bget_chunk(bchunk_t *chunk);
    bool operator()(const bmgrchunk *l, const bmgrchunk *r);
};

bool bmgrchunk::operator()(const bmgrchunk *l, const bmgrchunk *r)
{
    return l->b_index<r->b_index;
}

static std::set<bmgrchunk*, bmgrchunk> __s_mgrchunk;

static bmgrchunk*
bget_mgrchunk(int index)
{
    bmgrchunk ck;
    if (index == -1){
        return NULL;
    }
    ck.b_index = index;
    if (__s_mgrchunk.find(&ck) == __s_mgrchunk.end()){
        return NULL;
    }
    return *__s_mgrchunk.find(&ck);
}

bool
bmgrchunk::bget_chunk(bchunk_t *chunk)
{
    if (b_started >= b_length){
        return false;
    }
    chunk->b_index = b_index;
    chunk->b_start = b_started;
    chunk->b_length = BLOCK_SIZE;
    if (b_started+BLOCK_SIZE>=b_length){
        chunk->b_length = b_length-b_started;
    }
    b_started += chunk->b_length;
    return true;
}

static bmgrchunk* 
bset_bit(int index)
{
    bmgrchunk *chk = bget_mgrchunk(index);
	if (chk == NULL){
	   	int ubyte = __q_visited[index>>3];
	   	int b1= ubyte;
	   	ubyte |= (1<<((~index)&0x7));
	   	__q_visited[index>>3] = ubyte;
//printf("bit: %d(%d/%d) %02x %02x\n", index, index%8, index&0x7, b1, ubyte);
		chk = new bmgrchunk();
	   	chk->b_index = index;
	   	chk->b_started = 0;
	   	chk->b_length = 512*1024;
	   	__s_mgrchunk.insert(chk);
	}
    return chk;
}

bchunk_t *
bchunk_get(int index, unsigned char *bitset, int count)
{
    int i;
    static bchunk_t chunk;
    bmgrchunk *chk = bget_mgrchunk(index);
    if (chk!=NULL && chk->bget_chunk(&chunk)){
        return &chunk;
    }
    __q_visited.resize(count);
    for (i=0; i<count; i++){
        int idx = rand()%count;
        unsigned char ubit = bitset[idx]&~__q_visited[idx];
        if (ubit != 0){
            idx = (idx<<3)+7;
            while(0==(ubit&0x1)){
                ubit >>= 1;
                idx--;
            }
            chk = bset_bit(idx);
            if (chk!=NULL&&chk->bget_chunk(&chunk)){
                return &chunk;
            }
            assert(0);
        }
    }
    return NULL;
}
