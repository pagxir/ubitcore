#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "bthread.h"
#include "bsocket.h"
#include "bhttpd.h"

class bhttpd: public bthread
{
    public:
        bhttpd();
        virtual int bdocall(time_t timeout);

    private:
        bsocket b_listener;
        int b_state;
};

#ifndef NDEBUG
static time_t __time = 0;
static int __current = 0;
static int __together = 0;
static int __total[0x10] = { 0 };
#define URL_FULL "http://ftp.cdut.edu.cn/pub/slackware/10.1/slackware-10.1-install-d1.iso"
#define URL_PATH "/pub/slackware/10.1/slackware-10.1-install-d1.iso"
#define URL_HOST "ftp.cdut.edu.cn"
#endif

bhttpd::bhttpd()
{
    b_ident = "bhttpd";
    b_state = 0;
#ifndef NDEBUG
    __time = time(NULL);
#endif
}

int
bhttpd::bdocall(time_t timeout)
{
    int error=0;
    int state = b_state;
    char buffer[81920];
    const char title[] = "GET "URL_PATH" HTTP/1.0\r\n"
        "Host: "URL_HOST"\r\n"
        "Connection: Close\r\n"
        "\r\n"
        ;
    while(error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                error = b_listener.bconnect(URL_HOST);
                break;
            case 1:
                error = b_listener.bsend(title, strlen(title));
                break;
            case 2:
                error = b_listener.breceive(buffer, sizeof(buffer));
                break;
            case 3:
                if (error > 0){
#ifndef NDEBUG
                    __together += error;
                    __total[__current&0xF] += error;
                    if (time(NULL) != __time){
                        int total = 0;
                        for (int i=0; i<0x10; i++){
                            total += __total[i];
                        }
                        fprintf(stderr, "\r%9d %3d.%02d %s", __together,
                                (total>>14), ((total&0x3FFF)*25)>>12, URL_FULL);
                        __current++;
                        __time = time(NULL);
                        __total[__current&0xF] = 0;
                    }
                    state = 2;
#endif
                }
                break;
            default:
                printf("connection is close by remote peer!\n");
                return 0;
        }
    }
#ifndef NDEBUG
    assert(error==-1);
#endif
    return error;
}

static bhttpd __httpd;

int
bhttpd_start()
{
    __httpd.bwakeup();
    return 0;
}
