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

    private:
        bupdown &b_updown;
};

bupload_wrapper::bupload_wrapper(bupdown &updown):
    b_updown(updown)
{
}

int
bupload_wrapper::bdocall(time_t timeout)
{
    int error = b_updown.bupload(timeout);
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
    }else if (len==1 && buf[0]==BT_MSG_UNCHOCK){
        b_rchoke = BT_MSG_UNCHOCK;
    }else if (len==1 && buf[0]==BT_MSG_INTERESTED){
        b_rinterested = BT_MSG_INTERESTED;
    }else if (len==1 && buf[0]==BT_MSG_NOINTERESTED){
        b_rinterested = BT_MSG_NOINTERESTED;
    }else if (len==5 && buf[0]==BT_MSG_HAVE){
        unsigned long idx = ntohl(text[0]); 
        unsigned char flag = b_bitset[idx>>3];
        b_bitset[idx>>3] |= (1<<((~idx)&0x7));
        b_new_linterested = BT_MSG_INTERESTED;
    }else if (len>1 && buf[0]==BT_MSG_BITFIELD){
        b_bitset.resize(len-1);
        memcpy(&b_bitset[0], buf+1, len-1);
        b_new_linterested = BT_MSG_INTERESTED;
    }else if (len==13 && buf[0]==BT_MSG_REQUEST){
        printf("request: %d %d %d\n",
                ntohl(text[0]), ntohl(text[1]), ntohl(text[2]));
    }else if (len>9 && buf[0]==BT_MSG_PIECE){
        bchunk_sync((const char*)&text[2],
                ntohl(text[0]), ntohl(text[1]), len-9);
        b_requesting -= len;
        if (b_requesting < 0){
            b_requesting = 0;
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

    if (b_rchoke==BT_MSG_UNCHOCK && b_requesting<128*1024){
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

    return 0;
}

int
bupdown::bfailed()
{
    return bthread::bfailed();
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
                b_lastref = -1;
                b_keepalive = 0;
                b_requesting = 0;
                b_upoff = b_upsize = 0;
                b_rchoke = BT_MSG_CHOCK;
                b_rinterested = BT_MSG_NOINTERESTED;
                b_lchoke = BT_MSG_CHOCK;
                b_new_lchoke = BT_MSG_UNCHOCK;
                b_linterested = BT_MSG_NOINTERESTED;
                b_new_linterested = BT_MSG_NOINTERESTED;
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
                        b_upload->bwakeup();
                        b_offset -= parsed;
                        memmove(b_buffer, b_buffer+parsed, b_offset);
                    }
                    state = 2;
                }
                break;
            default:
                b_offset = 0;
                b_lastref = -1;
                b_keepalive = 0;
                b_requesting = 0;
                b_upoff = b_upsize = 0;
                b_rchoke = BT_MSG_CHOCK;
                b_rinterested = BT_MSG_INTERESTED;
                b_lchoke = BT_MSG_CHOCK;
                b_new_lchoke = BT_MSG_CHOCK;
                b_linterested = BT_MSG_NOINTERESTED;
                b_new_linterested = BT_MSG_INTERESTED;
                state = 0;
                break;
        }
    }

    return error;
}
