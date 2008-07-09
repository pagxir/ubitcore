/* $Id$ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

#include "bchunk.h"
#include "bupdown.h"

class bupload_wrapper: public bthread
{
    public:
        bupload_wrapper(bupdown &updown);
        virtual int bdocall(time_t timeout);
        bool bwakeup_safe();
        bool bglobal_wait();
        bool bglobal_wakeup();
        std::queue<bchunk_t> &queue(){ return b_queue; }

    private:
        bool b_flag;
        bool b_wakeupable;
        bupdown &b_updown;
        std::queue<bchunk_t> b_queue;
};

std::queue<bupload_wrapper*> __q_upload;

bool
bglobal_break()
{
    while (!__q_upload.empty()){
        __q_upload.front()->bglobal_wakeup();
        __q_upload.pop();
    }
    return true;
}

bupload_wrapper::bupload_wrapper(bupdown &updown):
    b_updown(updown), b_wakeupable(false)
{
}

bool
bupload_wrapper::bglobal_wait()
{
    if (b_flag == true){
        return false;
    }
    __q_upload.push(this);
    b_flag = true;
    return true;
}

bool
bupload_wrapper::bglobal_wakeup()
{
    assert(b_flag == true);
    b_flag = false;
    bwakeup_safe();
    return true;
}

bool
bupload_wrapper::bwakeup_safe()
{
    if (b_wakeupable){
        b_wakeupable = false;
        bwakeup();
    }
    return b_wakeupable;
}

int
bupload_wrapper::bdocall(time_t timeout)
{
    int error = b_updown.bupload(timeout);
    b_wakeupable = (error==0);
    if (b_wakeupable==true){
        bglobal_wait();
    }
    return error;
}

enum {
    BT_MSG_CHOCK,
    BT_MSG_UNCHOCK,
    BT_MSG_INTERESTED,
    BT_MSG_NOINTERESTED,
    BT_MSG_HAVE,
    BT_MSG_BITFIELD,
    BT_MSG_REQUEST,
    BT_MSG_PIECE,
    BT_MSG_CANCEL
};

int
bupdown::quick_decode(char *buf, int len, int readed)
{
    if (len>9 && buf[0]==BT_MSG_PIECE){
#if 0
        printf("quick data piece: index=%d begin=%d len=%d/%d\n", 
                ntohl(*(unsigned long*)(buf+1)),
                ntohl(*(unsigned long*)(buf+5)),
                readed, len
                );
#endif
    }
    return 0;
}

int
bupdown::real_decode(char *buf, int len)
{
    unsigned long* text =(unsigned long *)(buf+1);
    if (len == 0){
        b_keepalive = 1;
    }else if (len==1 && buf[0]==BT_MSG_CHOCK){
        b_rchoke = BT_MSG_CHOCK;
        b_requesting = 0;
    }else if (len==1 && buf[0]==BT_MSG_UNCHOCK){
        b_rchoke = BT_MSG_UNCHOCK;
    }else if (len==1 && buf[0]==BT_MSG_INTERESTED){
        b_rinterested = BT_MSG_INTERESTED;
    }else if (len==1 && buf[0]==BT_MSG_NOINTERESTED){
        b_rinterested = BT_MSG_NOINTERESTED;
    }else if (len==5 && buf[0]==BT_MSG_HAVE){
        unsigned long idx = ntohl(text[0]); 
        b_bitset.resize((idx>>3)+1);
        unsigned char flag = b_bitset[idx>>3];
        b_bitset[idx>>3] |= (1<<((~idx)&0x7));
        b_new_linterested = BT_MSG_INTERESTED;
    }else if (len>1 && buf[0]==BT_MSG_BITFIELD){
        b_bitset.resize(len-1);
        memcpy(&b_bitset[0], buf+1, len-1);
        b_new_linterested = BT_MSG_INTERESTED;
    }else if (len==13 && buf[0]==BT_MSG_REQUEST){
        int piece = htonl(text[0]);
        int start = htonl(text[1]);
        int length = htonl(text[2]);
        printf("request: %d %d %d\n", piece, start, length);
        if (b_lchoke == BT_MSG_UNCHOCK){
            b_upload->queue().push(
                    bchunk_t(piece, start, length));
        }
    }else if (len>9 && buf[0]==BT_MSG_PIECE){
        bchunk_sync((const char*)&text[2],
                ntohl(text[0]), ntohl(text[1]), len-9);
        b_requesting -= len;
        if (b_requesting < 0){
            b_requesting = 9;
        }
    }else if (len==13 && buf[0]==BT_MSG_CANCEL){
        printf("cancel: %d %d %d\n",
                ntohl(text[0]), ntohl(text[1]), ntohl(text[2]));
    }
    return 0;
}

bupdown::bupdown()
    :b_state(0), b_upload(new bupload_wrapper(*this))
{
    b_ident = "bupdown";
}

int
bupdown::bupload(time_t timeout)
{
    int error = 0;
again:
    while (b_upoff < b_upsize){
        error = b_ep.b_socket.bsend(b_upbuffer+b_upoff,
                b_upsize - b_upoff);
        if (error == -1){
            bdump_socket_crash_message("bupload");
            return -1;
        }
        b_upoff += error;
    }

    b_upoff = b_upsize = 0;
    if (b_keepalive==1 && b_upsize+4<sizeof(b_upbuffer)){
        memset(b_upbuffer+b_upsize, 0, 4);
        b_keepalive = 0;
        b_upsize += 4;
        goto again;
    }

    if (bget_have(b_ptrhave) != -1){
        unsigned long msglen = htonl(5);
        memcpy(b_upbuffer+b_upsize, &msglen, 4);
        b_upsize += 4;
        b_upbuffer[b_upsize] = BT_MSG_HAVE;
        b_upsize += 1;
        msglen = htonl(bget_have(b_ptrhave++));
        memcpy(b_upbuffer+b_upsize, &msglen, 4);
        b_upsize += 4;
        goto again;
    }

    if (b_lchoke!=b_new_lchoke && b_upsize+5<sizeof(b_upbuffer)){
        b_lchoke = b_new_lchoke;
        unsigned long msglen = htonl(1);
        memcpy(b_upbuffer+b_upsize, &msglen, 4);
        b_upsize += 4;
        b_upbuffer[b_upsize] = b_new_lchoke;
        b_upsize += 1;
        goto again;
    }

    if (b_linterested!=b_new_linterested && b_upsize+5<sizeof(b_upbuffer)){
        b_linterested = b_new_linterested;
        unsigned long msglen = htonl(1);
        memcpy(b_upbuffer+b_upsize, &msglen, 4);
        b_upsize += 4;
        b_upbuffer[b_upsize] = b_new_linterested;
        b_upsize += 1;
        goto again;
    }

    if (b_rchoke==BT_MSG_UNCHOCK && b_requesting<96*1024){
        if(b_bitset.size()==0){
            return 0;
        }
        bchunk_t *chunk = bchunk_get(b_lastref, &b_bitset[0], b_bitset.size());
        if (chunk != NULL){
            b_lastref = chunk->b_index;
            b_requesting += chunk->b_length;
            unsigned long msglen = htonl(12+1);
            memcpy(b_upbuffer+b_upsize, &msglen, 4);
            b_upsize += 4;
            b_upbuffer[b_upsize] = BT_MSG_REQUEST;
            b_upsize += 1;
            msglen = htonl(chunk->b_index);
            memcpy(b_upbuffer+b_upsize, &msglen, 4);
            b_upsize += 4;
            msglen = htonl(chunk->b_start);
            memcpy(b_upbuffer+b_upsize, &msglen, 4);
            b_upsize += 4;
            msglen = htonl(chunk->b_length);
            memcpy(b_upbuffer+b_upsize, &msglen, 4);
            b_upsize += 4;
#if 0
            printf("rq: %d %d %d\n", chunk->b_index,
                    chunk->b_start, chunk->b_length);
#endif
            goto again;
        }
    }

    if (!b_upload->queue().empty()){
        bchunk_t chunk = b_upload->queue().front();
        b_upload->queue().pop();
        unsigned long msglen = htonl(9+chunk.b_length);
        memcpy(b_upbuffer+b_upsize, &msglen, 4);
        b_upsize += 4;
        b_upbuffer[b_upsize] = BT_MSG_PIECE;
        b_upsize += 1;
        msglen = htonl(chunk.b_index);
        memcpy(b_upbuffer+b_upsize, &msglen, 4);
        b_upsize += 4;
        msglen = htonl(chunk.b_start);
        memcpy(b_upbuffer+b_upsize, &msglen, 4);
        b_upsize += 4;
        bchunk_copyto(b_upbuffer+b_upsize, &chunk);
        b_upsize += chunk.b_length;
        printf("upload data: %d\n", chunk.b_index);
        goto again;
    }

    return 0;
}

static int __cc = 0;

int
bupdown::bfailed()
{
    __cc--;
    b_state = 4;
    this->bwakeup();
    printf("fail: %d\n", __cc);
    return 0;
}

int
bupdown::bdocall(time_t timeout)
{
    int error = 0;
    int state = b_state;
    while (error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                b_ep.bclear();
                error = bready_pop(&b_ep);
                break;
            case 1:
                b_offset = 0;
                b_ptrhave = 0;
                b_lastref = -1;
                b_keepalive = 0;
                b_requesting = 0;
                b_upoff = b_upsize = 0;
                b_rchoke = BT_MSG_CHOCK;
                b_rinterested = BT_MSG_NOINTERESTED;
                b_lchoke = BT_MSG_CHOCK;
                b_new_lchoke = BT_MSG_UNCHOCK;
                b_linterested = BT_MSG_NOINTERESTED;
                b_new_linterested = BT_MSG_INTERESTED;
                b_upload->bwakeup();
                __cc++;
                break;
            case 2:
                error = b_ep.b_socket.breceive(b_buffer+b_offset,
                        sizeof(b_buffer)-b_offset);
                break;
            case 3:
                if (error > 0){
                    b_offset += error;
                    int parsed = 0;
                    int parsed4 = parsed + 4;
                    while (parsed4 <= b_offset){
                        unsigned long l = ntohl(*(unsigned long*)
                                (b_buffer+parsed));
                        if (b_offset < parsed4+l){
                            quick_decode(b_buffer+parsed4, l,
                                    b_offset-parsed4);
                            break;
                        }
                        real_decode(b_buffer+parsed4, l);
                        parsed = parsed4+l;
                        parsed4 = parsed+4;
                    }
                    if (parsed > 0){
                        b_upload->bwakeup_safe();
                        b_offset -= parsed;
                        memmove(b_buffer, b_buffer+parsed, b_offset);
                    }
                    state = 2;
                }
                break;
            default:
                printf("connection lost: %d\n", --__cc);
                b_offset = 0;
                b_lastref = -1;
                b_ptrhave = -1;
                b_keepalive = 0;
                b_requesting = 0;
                b_upoff = b_upsize = 0;
                b_rchoke = BT_MSG_CHOCK;
                b_rinterested = BT_MSG_INTERESTED;
                b_lchoke = BT_MSG_CHOCK;
                b_new_lchoke = BT_MSG_CHOCK;
                b_linterested = BT_MSG_INTERESTED;
                b_new_linterested = BT_MSG_INTERESTED;
                state = 0;
                break;
        }
    }

    return error;
}
