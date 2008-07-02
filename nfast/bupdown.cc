/* $Id$ */
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "bupdown.h"

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
quick_decode(char *buf, int len)
{
    if (len == 0){
        return 0;
    }
    if (len>9 && buf[0]==BT_MSG_PIECE){
        printf("quick data piece: index=%d begin=%d\n", 
                ntohl(*(unsigned long*)(buf+1)), ntohl(*(unsigned long*)(buf+5)));
    }
    return 0;
}

int
real_decode(char *buf, int len)
{
    unsigned long* text =(unsigned long *)(buf+1);
    if (len == 0){
        printf("keep alive\n");
    }else if (len==1 && buf[0]==BT_MSG_CHOCK){
        printf("chock\n");
    }else if (len==1 && buf[0]==BT_MSG_UNCHOCK){
        printf("unchock\n");
    }else if (len==1 && buf[0]==BT_MSG_INTERESTED){
        printf("interested\n");
    }else if (len==1 && buf[0]==BT_MSG_NOINTERESTED){
        printf("not interested\n");
    }else if (len==5 && buf[0]==BT_MSG_HAVE){
        printf("have: %d\n", ntohl(text[0])); 
    }else if (len>1 && buf[0]==BT_MSG_BITFIELD){
        printf("byte field: %d\n", len);
    }else if (len==13 && buf[0]==BT_MSG_REQUEST){
        printf("request: %d %d %d\n",
                ntohl(text[0]), ntohl(text[1]), ntohl(text[2]));
    }else if (len>9 && buf[0]==BT_MSG_PIECE){
        printf("piece: %d %d %d\n",
                ntohl(text[0]), ntohl(text[1]), len);
    }else if (len==13 && buf[0]==BT_MSG_CANCEL){
        printf("cancel: %d %d %d\n",
                ntohl(text[0]), ntohl(text[1]), ntohl(text[2]));
    }
    return 0;
}

bupdown::bupdown()
    :b_state(0), b_ep(NULL)
{
    b_ident = "bupdown";
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
                if (b_ep != NULL){
                    delete b_ep;
                    b_ep = NULL;
                }
                b_offset = 0;
                error = bready_pop(&b_ep);
                break;
            case 1:
                error = b_ep->b_socket.breceive(b_buffer+b_offset,
                        sizeof(b_buffer)-b_offset);
                break;
            case 2:
                if (error > 0){
                    b_offset += error;
                    while (b_offset >= 4){
                        unsigned long l = ntohl(*(unsigned long*)b_buffer);
                        unsigned long meat = l + 4;
                        if (b_offset < meat){
                            quick_decode(b_buffer+4, l);
                            break;
                        }
                        real_decode(b_buffer+4, l);
                        b_offset -= meat;
                        memmove(b_buffer, b_buffer+meat, b_offset);
                    }
                    state = 1;
                }
                break;
            case 3:
                printf("U: hello world!\n");
                break;
            default:
                state = 0;
                break;
        }
    }

    return error;
}
