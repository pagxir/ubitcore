#include <stdio.h>
#include <time.h>
#include <string.h>

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

bhttpd::bhttpd()
{
    b_state = 0;
}

int
bhttpd::bdocall(time_t timeout)
{
    int error=0;
    int state = b_state;
    char buffer[8192];
    const char title[] = "GET / HTTP/1.0\r\n\r\n";
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
                error = b_listener.breceive(buffer, sizeof(buffer)-1);
                break;
            case 3:
                if (error == 0){
                    printf("\n");
                    return 0;
                }
                buffer[error] = 0;
                printf("%s", buffer);
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
