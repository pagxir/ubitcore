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
#include "bitfield.h"

#define BLOCK_SIZE (16*1024)

static int __n_have = 0;
static std::vector<int> __q_have;

static int __bend_key = 0;
static int __piece_length;
static bitfield __q_visited;
static bitfield __q_finished;
static bitfield __q_syncfile;

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

static std::set<bmgrchunk*, bmgrchunk> __qfinish_list;
static std::set<bmgrchunk*, bmgrchunk> __qupdown_list;
static std::queue<bmgrchunk*> __qreuse_list;

static bmgrchunk *
balloc_chunk()
{
    bmgrchunk *chk = NULL;
    static int __count = 0;
    if (__count < 20){
        __count++;
        chk = new bmgrchunk();
        chk->b_buffer = new char[__piece_length];
        return chk;
    }
    if (!__qreuse_list.empty()){
        bmgrchunk *chunk = __qreuse_list.front();
        __qupdown_list.erase(chunk);
        __qreuse_list.pop();
		return chunk;
    }
    chk = new bmgrchunk();
    chk->b_buffer = new char[__piece_length];
    return chk;
}

static bmgrchunk*
bget_mgrchunk(int index)
{
    bmgrchunk ck;
    if (index == -1){
        return NULL;
    }
    ck.b_index = index;
    if (__qupdown_list.find(&ck) == __qupdown_list.end()){
        return NULL;
    }
    return *__qupdown_list.find(&ck);
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

int
bpost_chunk(int piece)
{
    bmgrchunk *chk = bget_mgrchunk(piece);
    if(chk == NULL){
        chk = balloc_chunk();
        chk->b_index = piece;
        chk->b_started = 0;
        chk->b_length = __piece_length;
        __qfinish_list.insert(chk);
    }
    return 0;
}

static bmgrchunk* 
bset_bit(int index)
{
    bmgrchunk *chk = bget_mgrchunk(index);
	if (chk == NULL){
        __bend_key = index;
	   	__q_visited.bitset(index);
		chk = balloc_chunk();
	   	chk->b_index = index;
        chk->b_badcount = 0;
	   	chk->b_started = 0;
        chk->b_recved = 0;
	   	chk->b_length = __piece_length;
	   	__qupdown_list.insert(chk);
	}
    return chk;
}


static int __bad_mask = 0;
static int __map_count = -1;
static int *__rnd_map = NULL;

int
bend_key()
{
    return __bend_key;
}

static int
build_map(int count)
{
    int i;
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
    return 0;
}

static int
rnd_map(int idx)
{
    int i =__rnd_map[idx];
    while (__q_finished.bitget(i) && __bad_mask>0){
        __bad_mask--;
        __rnd_map[idx] = __rnd_map[__bad_mask];
        i =__rnd_map[idx];
    }
    return __q_visited.bitget(i)?-1:i;
}
bchunk_t *
bchunk_get(int index, bitfield &bitset, int *lidx,
        int *endkey, int *lcount, int *bhave)
{
    int i;
    static bchunk_t chunk;
    bmgrchunk *chk = bget_mgrchunk(index);
    if (chk!=NULL && chk->bget_chunk(&chunk)){
        return &chunk;
    }
retry:
	if (*lcount > 1){
	   	for (i=(*lidx)%(*lcount); i!=*endkey; i=(i+1)%(*lcount)){
		   	int rnd = rnd_map(i);
		   	if (rnd!=-1 && bitset.bitget(rnd)){
			   	chk = bset_bit(rnd);
			   	assert(chk != NULL);
			   	if (chk!=NULL&&chk->bget_chunk(&chunk)){
				   	*lidx = i;
				   	return &chunk;
			   	}
		   	}
	   	}
	}
    *lcount = __bad_mask;
    if (*bhave == 0){
        printf("bget chunk fail\n");
        return NULL;
    }
    *bhave = 0;
    *endkey = bend_key();
    *lidx = 1+*endkey;
    goto retry;
    return NULL;
}

static size_t __count_of_piece = 0;

int
bset_piece_info(int length, int count, int rest)
{
    int pc = rest>0?count+1:count;
    build_map(pc);
    __q_visited.resize(pc);
    __q_finished.resize(pc);
    __q_syncfile.resize(pc);
    __piece_length = length;
    __count_of_piece = pc;
    if (rest != 0){
        bmgrchunk *chk = new bmgrchunk();
	   	chk->b_index = count;
        chk->b_badcount = 0;
	   	chk->b_started = 0;
        chk->b_recved = 0;
	   	chk->b_length = __piece_length;
        chk->b_buffer = new char[rest];
	   	__qupdown_list.insert(chk);
    }
    return 0;
}

size_t
bmap_count()
{
    return __bad_mask;
}

size_t
bcount_piece()
{
    return __count_of_piece;
}

int bchunk_sync(const char *buf, int idx, int start, int len)
{   
    if (__q_finished.bitget(idx)){
        return len;
    }
    bmgrchunk *chk = bget_mgrchunk(idx);
    assert(chk!=NULL);
    assert(len>0&&start>=0);
    assert(start+len <= chk->b_length);
    assert(chk->b_buffer != NULL);
    memcpy(chk->b_buffer+start, buf, len);
    std::map<int,int> &lm = chk->b_lostmap;
    if (start > chk->b_recved){
        if (lm.find(start) == lm.end()){
            lm.insert(std::make_pair(start, len));
		}else if (lm.find(start)->second > len){
			lm.insert(std::make_pair(start, len));
		}
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
        __q_finished.bitset(idx);
        bglobal_break();
#if 0
        delete[] chk->b_buffer;
        chk->b_buffer = NULL;
#endif
        __qfinish_list.insert(chk);
    }
    return len;
}

int
bchunk_copyto(char *buf, bchunk_t *chunk)
{
    bmgrchunk *chk = bget_mgrchunk(chunk->b_index);
    if(chk == NULL){
        bpost_chunk(chunk->b_index);
        return -1;
    }
    assert(chunk->b_start >= 0);
    assert(chunk->b_start+chunk->b_length <= chk->b_recved);
    memcpy(buf, chk->b_buffer+chunk->b_start, chunk->b_length);
    return chunk->b_length;
}

int
bcancel_request(int idx)
{
    __q_visited.bitclr(idx);
    bmgrchunk *chk = bget_mgrchunk(idx);
    if (chk != NULL){
        chk->b_started = chk->b_recved;
    }
    return 0;
}

int
bfile_sync(FILE *fp, int piece, int start)
{
    int count = 0;
    static int __lpiece = 0;
    static int __lstart = 0;
    for (std::set<bmgrchunk*, bmgrchunk>::iterator pchunk
            = __qfinish_list.begin(); 
           pchunk!=__qfinish_list.end();
           __qfinish_list.erase(pchunk++)){
        int offset = 0;
        int length = (*pchunk)->b_length;
        if ((*pchunk)->b_index > piece){
            break;
        }
        if ((*pchunk)->b_index == __lpiece){
            offset = __lstart;
        }
        if ((*pchunk)->b_index == piece){
            length = start;
        }
        if(fp == NULL){
            __lpiece=__lstart=0;
			return 0;
		}
        if (__q_syncfile.bitget((*pchunk)->b_index)){
            printf("tomem: %p %d @ %d %d\n",
                    (*pchunk)->b_buffer+offset,
                    length-offset,
                    (*pchunk)->b_index-__lpiece,
                    offset-__lstart
                    );
            if (__qupdown_list.find(*pchunk) == __qupdown_list.end()){
                fseek(fp, __piece_length*((*pchunk)->b_index-__lpiece)
                        +(offset-__lstart), SEEK_SET);
                int n=fread((*pchunk)->b_buffer+offset,
                        1, length-offset, fp);
                (*pchunk)->b_recved = __piece_length;
                (*pchunk)->b_started = __piece_length;
                __qupdown_list.insert(*pchunk);
                assert(n);
            }
        }else{
            printf("tofile: %p %d @ %d %d\n",
                    (*pchunk)->b_buffer+offset,
                    length-offset,
                    (*pchunk)->b_index-__lpiece,
                    offset-__lstart
                    );
            fseek(fp, __piece_length*((*pchunk)->b_index-__lpiece)
                    +(offset-__lstart), SEEK_SET);
            int n=fwrite((*pchunk)->b_buffer+offset,
                    1, length-offset, fp);
            assert(n>=0);
        }
        if ((*pchunk)->b_index == piece){
            break;
        }
        __q_syncfile.bitset((*pchunk)->b_index);
        if ((*pchunk)->b_length == __piece_length){
            __qreuse_list.push(*pchunk);
        }
    }
    __lpiece = piece;
    __lstart = start;
    return __qfinish_list.empty();
}

int
bsync_bitfield(char *buffer, int *have)
{
    *have = __n_have;
    return __q_finished.bcopyto(
            (unsigned char*)buffer,
            __q_finished.byte_size());
}
