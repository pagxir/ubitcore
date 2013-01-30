#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "bthread.h"
#include "bsocket.h"
#include "burlget.h"

class burlget_wrapper: public burlget
{
    public:
        burlget_wrapper();
        virtual int boutbind(boutfile *bfile);
        virtual int burlbind(const char *url);
        virtual int bpolldata(char *buf, int len);

    private:
        bsocket b_connect;
        int b_len;
        int b_state;
        int b_urlport;
        char b_urlhost[128];
        char b_urlfull[512];
        char b_bufhead[1024];
};

burlget *
burlget::get()
{
    return new burlget_wrapper();
}

burlget::~burlget()
{
}

#ifndef NDEBUG
static time_t __time = 0;
static int __current = 0;
static int __together = 0;
static int __total[0x10] = { 0 };
#endif

#define URL_PATH "%s"
#define URL_HOST "%s"
#define USER_AGENT "url-get/0.1"
#define ACCEPT_TYPE "*/*"

burlget_wrapper::burlget_wrapper()
{
    b_len = 0;
    b_state = 0;
    b_urlport = 80;
#ifndef NDEBUG
    __time = time(NULL);
#endif
}

int
burlget_wrapper::boutbind(boutfile *bfile)
{
}

int
burlget_wrapper::burlbind(const char *url)
{
    int i=0;
    const char *urlpath = url+7;
    const char title[] = "GET "URL_PATH" HTTP/1.0\r\n"
        "Host: "URL_HOST"\r\n"
        "User-Agent: "USER_AGENT"\r\n"
        "Accept: "ACCEPT_TYPE"\r\n"
        "Connection: Close\r\n"
        "\r\n"
        ;
    assert(url != NULL);
    assert(strlen(url)+1 < sizeof(b_urlfull));
    assert(strncmp(url, "http://", 7)==0);
    strcpy(b_urlfull, url);
    while(*urlpath!='/' && *urlpath){
        assert(i+2<sizeof(b_urlhost));
        b_urlhost[i++] = *urlpath++;
    }
    b_urlhost[i] = 0;
    if (*urlpath==0){
        urlpath = "/";
    }
    sprintf(b_bufhead, title,
            urlpath,
            b_urlhost
           );
    char *chdot = strrchr(b_urlhost, ':');
    if (chdot != NULL){
        *chdot = 0;
        b_urlport = atoi(chdot+1);
    }
    return 0;
}

int
burlget_wrapper::bpolldata(char *buffer, int size)
{
    int i;
    int error=0;
    int state = b_state;
    const char match[5]="\r\n\r\n";
    while(error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                error = b_connect.bconnect(b_urlhost, b_urlport);
                break;
            case 1:
                error = b_connect.bsend(b_bufhead, strlen(b_bufhead));
                break;
            case 2:
                error = b_connect.breceive(buffer, size);
                break;
            case 3:
                for (i=0; i<error&&b_len<4; i++){
                    if (buffer[i] == match[b_len]){
                        b_len ++;
                    }else{
                        b_len = (buffer[i]=='\r');
                    }
                }
                if (error > 0 && b_len<4){
                    printf("not finish: %s\n", buffer);
                    state = 2;
                    break;
                }
                if (error > i){
                    memmove(buffer, buffer+i, error-i);
                }
                error-=i;
                break;
            case 4:
                break;
            case 5:
                if (error > 0){
                    return error;
                }
                error = b_connect.breceive(buffer, size);
                break;
            case 6:
                if (error > 0){
                    state = 5;
#ifndef NDEBUG
                    __together += error;
                    __total[__current&0xF] += error;
                    if (time(NULL) != __time){
                        int total = 0;
                        for (int i=0; i<0x10; i++){
                            total += __total[i];
                        }
                        fprintf(stderr, "\r%9d %3d.%02d %s", __together,
                                (total>>14), ((total&0x3FFF)*25)>>12,
                                b_urlfull);
                        __current++;
                        __time = time(NULL);
                        __total[__current&0xF] = 0;
                    }
#endif
                }
                break;
            default:
                printf("remote close: %s %d !\n", b_urlfull, error);
                return error;
        }
    }
    return error;
}
