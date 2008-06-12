#include <stdio.h>
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
#endif

bhttpd::bhttpd()
{
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
    const char title[] = "GET /os/FreeBSD/snapshots/200805/8.0-CURRENT-200805-i386-disc1.iso HTTP/1.0\r\n\r\n";
    while(error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                error = b_listener.bconnect();
                break;
            case 1:
                error = b_listener.bsend(title, strlen(title));
                break;
            case 2:
                error = b_listener.breceive(buffer, sizeof(buffer));
                break;
            case 3:
                if (error == 0){
                    printf("connection close\n");
                    return 0;
                }
#ifndef NDEBUG
                __together += error;
                __total[__current&0xF] += error;
                if (time(NULL) != __time){
                    int total = 0;
                    for (int i=0; i<0x10; i++){
                        total += __total[i];
                    }
                    fprintf(stderr, "\r%5d %d", total>>4, __together);
                    __current++;
                    __time = time(NULL);
                    __total[__current&0xF] = 0;
                }
#endif
                state = 2;
                break;
            default:
                error = -1;
                break;
        }
    }
    return error;
}

static bhttpd __httpd;

int
bhttpd_start()
{
    __httpd.bwakeup();
    return 0;
}
