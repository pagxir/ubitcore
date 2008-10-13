/* $Id$ */
#include <time.h>
#include <string.h>
#include <assert.h>
#include <memory>
#include <set>

#include "sha1.h"
#include "bfiled.h"
#include "bupdown.h"
#include "bchunk.h"

static std::set<bfile_info> __qfile_list;
static unsigned char *__piece_hash;

int
bset_piece_hash(const void *hash, size_t len)
{
    __piece_hash = new unsigned char[len];
    memcpy(__piece_hash, hash, len);
    return 0;
}

int
badd_per_file(int piece, int start, const char *path)
{
#if 1
    return 0;
#endif
    FILE *fp= fopen(path, "rb+");
    if (fp == NULL){
        fp = fopen(path, "wb+");
    }
    if(__qfile_list.find(bfile_info(piece, start, fp))
            != __qfile_list.end()){
        fclose(fp);
        return 0;
    }
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

static int hash_check(int piece, void *buf, size_t len)
{
    int i;
    unsigned char digest[20];
    SHA1Hash(digest, static_cast<unsigned char*>(buf), len);
    if (0==memcmp(digest, __piece_hash+20*piece, 20)){
        bset_finish(piece);
    }
    return 0;
}

int fmsync(FILE *, void *, size_t, int, int);
static char __buffer[1024*1024*2];
int
bthash_check(void)
{
    int count = 0;
    std::set<bfile_info> &filelist = __qfile_list;
    std::set<bfile_info>::iterator file_iterator;
    file_iterator = filelist.begin();

    int from_base = 0;
    int piece_base = 0;
    int start_base = 0;
    int piece_number = 0;
    for (;;){
        if (file_iterator==filelist.end()){
            break;
        }
        count = 0;
        const bfile_info &info = *file_iterator;
        if (info.b_piece < piece_number){
            piece_base = info.b_piece;
            start_base = info.b_start;
            file_iterator++;
        }else if(piece_number < info.b_piece){
            int length = get_piece_length();
            count = fmsync(info.b_fd, 
                    __buffer+from_base,
                    length-from_base,
                    piece_number-piece_base,
                    from_base-start_base);
            if (count+from_base<length){
                piece_number = info.b_piece;
            }else{
                hash_check(piece_number, __buffer, length);
                piece_number++;
            }
            from_base = 0;
        }else if (info.b_start < get_piece_length()){
            int length = info.b_start;
            count = fmsync(info.b_fd, 
                    __buffer+from_base,
                    length-from_base,
                    piece_number-piece_base,
                    from_base-start_base);
            from_base = info.b_start;
            piece_base = info.b_piece;
            start_base = info.b_start;
            file_iterator++;
        }else {
            int length = get_piece_length();
            count = fmsync(info.b_fd, 
                    __buffer+from_base,
                    length-from_base,
                    piece_number-piece_base,
                    from_base-start_base);
            hash_check(piece_number, __buffer, length);
            from_base = 0;
            piece_number++;
        }
    }
    return 0;
}


int fmsync(FILE *fp, void *buf, size_t length, int blkno, int offset)
{
    size_t blksize = get_piece_length();
    int error = fseek(fp, blksize*blkno+offset, SEEK_SET);
    if (error == -1){
        return -1;
    }
    int count=fread(buf, 1, length, fp);
    return count;
}
