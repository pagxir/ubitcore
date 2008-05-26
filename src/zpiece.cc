#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include "sha1.h"
#include "zpiece.h"

#define _K(a) ((a)*1024)

extern int __last_piece_size;
struct piece_t {
    piece_t *pi_next;
    segment_t *pi_segfirst;
    const void *pi_lastid;
    int pi_number;
    int pi_rcved;
    int pi_length;
    int pi_requested;
    char pi_buffer[1];
};

extern int __piece_count;
extern int __piece_size;
extern char __bitField[1024];
extern char __bitTransfer[1024];
static piece_t *__pifirst = NULL;
piece_t *assign_piece(const char *bitfield);
int large_file_write(int piece, int start, const char *buf, int length);

piece_t *alloc_piece(int number, int length)
{
    piece_t *piece = NULL;
    for (piece=__pifirst; piece != NULL; piece=piece->pi_next) {
        if (piece->pi_number == number) {
            return piece;
        }
    }
    piece_t *pi = (piece_t*)malloc(sizeof(piece_t)+length);
    pi->pi_segfirst = NULL;
    pi->pi_rcved = 0;
    pi->pi_requested = 0;
    pi->pi_number = number;
    pi->pi_length = (number<__piece_count)?
                    length:__last_piece_size;
    pi->pi_next = __pifirst;
    __pifirst = pi;
    return pi;
}

static int __downloaded = 0;
static segment_t *__segbadqueue = NULL;
static segment_t *__segtqueue = NULL;

static int dumpHash(char *buf, int len)
{
    int i;
    SHA1_CTX sha1Ctx;
    unsigned char digest[20];
    SHA1Init(&sha1Ctx);
    SHA1Update(&sha1Ctx, (unsigned char*)buf, len);
    SHA1Final(digest, &sha1Ctx);
    for (i=0; i<20; i++) {
        printf("%02x", digest[i]&0xff);
    }
    return 0;
}

int segbuf_write(int piece, int offset, const char buffer[], int len, const char *bitfield)
{
    piece_t *pi = NULL;
    segment_t *seg = NULL;
    piece_t **tail = &__pifirst;

    for (pi=__pifirst; pi!=NULL; pi=pi->pi_next) {
        if (pi->pi_number == piece) {
            break;
        }
        tail = &pi->pi_next;
    }
    if (pi == NULL) {
        return -1;
    }
    if (offset+len > pi->pi_length) {
        return -1;
    }
    segment_t **segtail = &__segbadqueue;
    segment_t *nextseg = NULL;
    for (seg=__segbadqueue; seg!=NULL; seg=nextseg) {
        nextseg = seg->seg_next;
        if (seg->seg_piece==piece && seg->seg_offset==offset) {
            *segtail = seg->seg_next;
            delete seg;
            continue;
        }
        if (seg->seg_lastid==bitfield) {
            *segtail = seg->seg_next;
            seg->seg_next = __segtqueue;
            __segtqueue = seg;
            continue;
        }
        segtail = &seg->seg_next;
    }
    memmove(pi->pi_buffer+offset, buffer, len);
    if (pi->pi_rcved < offset) {
        segment_t **tail = &pi->pi_segfirst;
        for (seg=pi->pi_segfirst; seg!=NULL; seg=seg->seg_next) {
            if (seg->seg_offset > offset) {
                break;
            }
            tail = &seg->seg_next;
        }
        segment_t *newseg = new segment_t();
        newseg->seg_next = seg;
        newseg->seg_offset = offset;
        newseg->seg_length = len;
        *tail = newseg;
        return 0;
    }
    int limited  = offset+len;
    pi->pi_rcved = pi->pi_rcved>limited?pi->pi_rcved:limited;
    segment_t *segnext= NULL;
    for (seg=pi->pi_segfirst; seg!=NULL; seg=segnext) {
        if (seg->seg_offset > pi->pi_rcved) {
            return 0;
        }
        limited = seg->seg_offset+seg->seg_length;
        pi->pi_rcved = limited>pi->pi_rcved?limited:pi->pi_rcved;
        segnext = pi->pi_segfirst = seg->seg_next;
        delete seg;
    }
    if (pi->pi_rcved == pi->pi_length) {
        *tail = pi->pi_next;
        fprintf(stderr, "\rpiece download finish: %d!", pi->pi_number);
        bitfield_set(__bitField, pi->pi_number);
        large_file_write(pi->pi_number,
                         0, pi->pi_buffer, pi->pi_length);
        __downloaded += pi->pi_length;
        dumpHash(pi->pi_buffer, __piece_size);
        printf("\n");
        free(pi);
    }
    return 0;
}


int bitfield_set(char bitField[], int index)
{
    if (index < __piece_count) {
        unsigned char flag = bitField[index/8];
        flag = flag|(0x80>>(index%8));
        bitField[index/8] = flag;
    }
    return 0;
}

int bitfield_clr(char bitField[], int index)
{
    if (index>=0 && index < __piece_count) {
        unsigned char flag = bitField[index/8];
        flag = flag&~(0x80>>(index%8));
        bitField[index/8] = flag;
    }
    return 0;
}

int bitfield_test(const char bitField[], int index)
{
    if (index>=0 && index < __piece_count) {
        unsigned char flag = bitField[index/8];
        return flag&(0x80>>(index%8));
    }
    return 1;
}

static void free_segment(segment_t *p)
{
    //printf("freeseg: piece(%d) offset(%d) length(%d)\n", p->seg_piece, p->seg_offset,p->seg_length);
}

static void dump_segment(segment_t *p, int line)
{
    //printf("segreq: piece(%d) offset(%d) length(%d) %d\n", p->seg_piece, p->seg_offset,p->seg_length, line);
}

int assign_segment(segment_t *segment, const char *bitfield)
{
    piece_t *pi;
    segment_t *seg, **plastseg = &__segbadqueue;

    for (seg=__segbadqueue; seg!=NULL; seg=seg->seg_next) {
        if (bitfield_test(bitfield, seg->seg_piece)
                && seg->seg_lastid==bitfield) {
            *plastseg = seg->seg_next;
            seg->seg_next = __segtqueue;
            __segtqueue = seg;
            seg->seg_lastid = bitfield;
            *segment = *seg;
            dump_segment(segment, __LINE__);
            return 0;
        }
        plastseg = &seg->seg_next;
    }

    for (pi=__pifirst; pi!=NULL; pi=pi->pi_next) {
        if (pi->pi_lastid==bitfield && bitfield_test(bitfield, pi->pi_number)) {
            if (pi->pi_requested < pi->pi_length) {
                segment->seg_piece = pi->pi_number;
                segment->seg_offset = pi->pi_requested;
                segment->seg_length =
                    pi->pi_requested+_K(16)<pi->pi_length?
                    _K(16):pi->pi_length-pi->pi_requested;
                pi->pi_requested += _K(16);
                segment_t *keep = new segment_t(*segment);
                keep->seg_lastid = bitfield;
                keep->seg_next = __segtqueue;
                __segtqueue = keep;
                dump_segment(segment, __LINE__);
                return 0;
            } else {
                plastseg = &__segbadqueue;
                for (seg=__segbadqueue; seg!=NULL; seg=seg->seg_next) {
                    if (bitfield_test(bitfield, seg->seg_piece)
                            && seg->seg_piece==pi->pi_number) {
                        *plastseg = seg->seg_next;
                        seg->seg_next = __segtqueue;
                        __segtqueue = seg;
                        seg->seg_lastid = bitfield;
                        *segment = *seg;
                        dump_segment(segment, __LINE__);
                        return 0;
                    }
                    plastseg = &seg->seg_next;
                }
            }
        }
    }

    plastseg = &__segbadqueue;
    for (seg=__segbadqueue; seg!=NULL; seg=seg->seg_next) {
        if (bitfield_test(bitfield, seg->seg_piece)) {
            *plastseg = seg->seg_next;
            seg->seg_next = __segtqueue;
            __segtqueue = seg;
            seg->seg_lastid = bitfield;
            *segment = *seg;
            dump_segment(segment, __LINE__);
            return 0;
        }
        plastseg = &seg->seg_next;
    }
    piece_t * piece = assign_piece(bitfield);
    if (piece != NULL) {
        segment->seg_offset = piece->pi_requested;
        segment->seg_length = _K(16);
        segment->seg_piece = piece->pi_number;
        piece->pi_lastid = bitfield;
        piece->pi_requested += _K(16);
        segment_t *keep = new segment_t(*segment);
        keep->seg_lastid = bitfield;
        keep->seg_next = __segtqueue;
        __segtqueue = keep;
        dump_segment(segment, __LINE__);
        return 0;
    }
    return -1;
}

int reassign_segment(const char *id)
{
    segment_t *segt=__segtqueue, *segnext;
    __segtqueue = NULL;
    for (; segt!=NULL; segt=segnext) {
        segnext = segt->seg_next;
        if (bitfield_test(__bitField, segt->seg_piece)) {
            delete segt;
            continue;
        }
        if (segt->seg_lastid == id) {
            segt->seg_time = time(NULL);
            segt->seg_next = __segbadqueue;
            __segbadqueue = segt;
            free_segment(segt);
        } else {
            segt->seg_next = __segtqueue;
            __segtqueue = segt;
        }
    }
    return 0;
}


piece_t *assign_piece(const char *bitfield)
{
    int i;
    piece_t *pi;
    int count = 0;

    memset(__bitTransfer, 0, (__piece_count+8)/8);

    for (pi=__pifirst; pi != NULL; pi=pi->pi_next) {
        if (bitfield_test(bitfield, pi->pi_number)) {
            if (pi->pi_requested < pi->pi_length) {
                return pi;
            }
        }
        bitfield_set(__bitTransfer, pi->pi_number);
    }

    int *piecelist = new int[__piece_count];
    for (i=0; i<__piece_count; i++) {
        if (bitfield_test(bitfield, i)
                && !bitfield_test(__bitField, i)
                && !bitfield_test(__bitTransfer, i)) {
            piecelist[count++] = i;
        }
    }

    int index = __piece_count;
    if (__last_piece_size>0
            && bitfield_test(bitfield, index)
            && !bitfield_test(__bitField, index)
            && !bitfield_test(__bitTransfer, index)) {
        piecelist[count++] = index;
    }

    if (count == 0) {
        delete[] piecelist;
        return NULL;
    }
    int retval = piecelist[rand()%count];
    delete[] piecelist;
    /* Notice: the last block would be not __piece_size */
    return alloc_piece(retval, __piece_size);
}


int get_downloaded()
{
    return __downloaded;
}

int get_left()
{
    int i;
    int count = 0;
    for (i=0; i<__piece_count; i++) {
        if (!bitfield_test(__bitField, i)) {
            count ++;
        }
    }
    return count*__piece_size+(bitfield_test(__bitField, i)?0:__last_piece_size);
}
