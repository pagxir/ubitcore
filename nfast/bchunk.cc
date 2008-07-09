/* $Id$ */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <vector>
#include <map>
#include <set>

#include "bchunk.h"
#include "bupdown.h"
#define BLOCK_SIZE (16*1024)

static int __piece_length;
static int __n_have = 0;
static std::vector<int> __q_have;
static std::vector<unsigned char> __q_visited;
static std::vector<unsigned char> __q_finished;

struct bmgrchunk
{
    int b_index;
    int b_started;
    int b_length;

    int b_recved;
    int b_badcount;
    char *b_buffer;
    std::map<int, int> b_lostmap;

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

int
bget_have(int idx)
{
    if (idx < 0){
        return -1;
    }
    if (idx < __n_have){
        return __q_have[idx];
    }
    return -1;
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
        chk->b_badcount = 0;
	   	chk->b_started = 0;
        chk->b_recved = 0;
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
            if (i > 1){
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
    for (i=last_use+1; (last_use%count)!=(i%count); i++){
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
        __last_use = i%count;
    }
    printf("bget chunk fail\n");
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
    assert(len>0&&start>=0);
    assert(start+len <= chk->b_length);
    assert(chk->b_buffer != NULL);
    memcpy(chk->b_buffer+start, buf, len);
    std::map<int,int> &lm = chk->b_lostmap;
    if (start > chk->b_recved){
        assert(lm.find(start) == lm.end());
        lm.insert(std::make_pair(start, len));
        chk->b_badcount++;
        return len;
    }
    chk->b_recved = std::max(start+len, chk->b_recved);
    std::map<int,int>::iterator iter;
    for (iter=lm.begin();
            iter != lm.end();
            lm.erase(iter++)){
        if (iter->first > chk->b_recved){
            return len;
        }
        int recved = iter->first+iter->second;
        chk->b_recved = std::max(recved, chk->b_recved);
    }
    if (chk->b_recved == chk->b_length){
        fprintf(stderr, "chunk finish: %d %d!\n", idx, chk->b_badcount);
        __q_have.push_back(idx);
        __n_have++;
        bglobal_break();
#if 0
        delete[] chk->b_buffer;
        chk->b_buffer = NULL;
#endif
    }
    return len;
}

int
bchunk_copyto(char *buf, bchunk_t *chunk)
{
    bmgrchunk *chk = bget_mgrchunk(chunk->b_index);
    assert(chk != NULL);
    assert(chunk->b_start >= 0);
    assert(chunk->b_start+chunk->b_length <= chk->b_recved);
    memcpy(buf, chk->b_buffer+chunk->b_start, chunk->b_length);
    return chunk->b_length;
}
