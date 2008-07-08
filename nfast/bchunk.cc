/* $Id:$ */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <vector>
#include <set>

#include "bchunk.h"
#define BLOCK_SIZE (16*1024)

static int __piece_length;
static std::vector<unsigned char> __q_visited;

struct bmgrchunk
{
    int b_index;
    int b_started;
    int b_length;

    int b_recved;
    char *b_buffer;
    unsigned int b_window;

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
		chk = new bmgrchunk();
	   	chk->b_index = index;
	   	chk->b_started = 0;
        chk->b_recved = 0;
        chk->b_window = 0;
	   	chk->b_length = __piece_length;
        chk->b_buffer = new char[__piece_length];
	   	__s_mgrchunk.insert(chk);
	}
    return chk;
}

static int
rnd_map(int idx, int count)
{
    int i;
    static int __bad_mask = 0;
    static int __map_count = -1;
    static int *__rnd_map = NULL;
    if (__map_count != count){
        delete[] __rnd_map;
        __rnd_map = new int[count];
        for (i=0; i<count; i++){
            __rnd_map[i] = i;
            if (i > 0){
                int u = rand()%i;
                __rnd_map[i] = __rnd_map[u];
                __rnd_map[u] = i;
            }
        }
        __map_count = count;
        __bad_mask = count;
    }
    if (__bad_mask == 0){
        return -1;
    }
    i =__rnd_map[idx%__bad_mask];
    while (__q_visited[i]==0xFF && __bad_mask>1){
        __bad_mask--;
        __rnd_map[i] = __rnd_map[__bad_mask];
        i =__rnd_map[idx%__bad_mask];
    }
    return i;
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
    static int __last_use = 0;
    __q_visited.resize(count);
    int last_use = __last_use;
    for (i=last_use+1; last_use!=(i%count); i++){
        int rnd = rnd_map(i, count);
        unsigned char ubit = bitset[rnd]&~__q_visited[rnd];
        if (ubit != 0){
            int idx = (rnd<<3)+7;
            while(0==(ubit&0x1)){
                ubit >>= 1;
                idx--;
            }
            chk = bset_bit(idx);
            assert(chk != NULL);
            if (chk!=NULL&&chk->bget_chunk(&chunk)){
                return &chunk;
            }
        }
        __last_use = (i+1)%count;
    }
    return NULL;
}

int
bset_piece_length(int length)
{
    printf("piece length: %d\n", length);
    __piece_length = length;
    return 0;
}

int bchunk_sync(const char *buf, int idx, int start, int len)
{
    bmgrchunk *chk = bget_mgrchunk(idx);
    assert(chk != NULL);
    assert(len > 0);
    assert(start >= 0);
    assert(start+len <= chk->b_length);
    if (chk->b_recved == start){
        memcpy(chk->b_buffer+start, buf, len);
        chk->b_recved += len;
    }else{
        printf("cache fail: idx=%d start=%d recv=%d\n", 
                idx, start, chk->b_recved);
        return 0;
    }
    if (chk->b_recved == chk->b_length){
        delete[] chk->b_buffer;
        chk->b_buffer = NULL;
        printf("chunk finish: %d!\n", idx);
    }
    return len;
}
