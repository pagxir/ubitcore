/* $Id$ */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <string>

#include "bthread.h"
#include "bsocket.h"
#include "burlget.h"

class burlget_wrapper: public burlget
{
    public:
        burlget_wrapper();
        virtual const char* blocation();
        virtual int burlbind(const char *url);
        virtual int bpolldata(char *buf, int len);

    private:
        bsocket b_connect;
        int b_idx;
        int b_len;
        int b_loc;
        int b_state;
        int b_urlport;
        char b_urlhost[256];
        char b_bufhead[1024];
        std::string b_urlfull;
};

burlget *
burlget::get()
{
    return new burlget_wrapper();
}

burlget::~burlget()
{
}

#define URL_PATH "%s"
#define URL_HOST "%s"
#define USER_AGENT "url-get/0.1"
#define ACCEPT_TYPE "*/*"

burlget_wrapper::burlget_wrapper()
{
    b_idx = 0;
    b_len = 0;
    b_loc = 0;
    b_state = 0;
    b_urlport = 80;
}

const char*
burlget_wrapper::blocation()
{
    if (b_loc == 12){
        return b_urlfull.c_str();
    }
    return NULL;
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
#ifndef NDEBUG
    if (strncmp(url, "http://", 7) != 0){
        printf("bad url: %s\n", url);
    }
#endif
    assert(strncmp(url, "http://", 7)==0);
    b_urlfull = url;
    while(*urlpath!='/' && *urlpath){
        assert(i+2<sizeof(b_urlhost));
        b_urlhost[i++] = *urlpath++;
    }
    b_urlhost[i] = 0;
    if (*urlpath==0){
        urlpath = "/";
    }
    sprintf(b_bufhead, title, urlpath, b_urlhost);
#ifdef DISABLE_PROXY
//    sprintf(b_bufhead, title, url, b_urlhost);
#endif
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
    const char mlocate[12]="\r\nLocation:";
    while(error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                error = b_connect.bconnect(b_urlhost, b_urlport);
#ifdef DISABLE_PROXY
//              error = b_connect.bconnect("121.14.55.8", 80);
#endif
                break;
            case 1:
                error = b_connect.bsend(b_bufhead, strlen(b_bufhead));
                break;
            case 2:
                error = b_connect.breceive(buffer, size);
                break;
            case 3:
                for (i=0; i<error&&b_len<4&&b_idx<2; i++){
		    if (buffer[i] == '\n'){
		        b_idx++;
		    }else{
			b_idx=0;
		    }
                    if (buffer[i] == match[b_len]){
                        b_len ++;
                    }else{
                        b_len = (buffer[i]=='\r');
                    }
                    if (b_loc < 11){ 
                        if (buffer[i]==mlocate[b_loc]){
                            b_loc++;
                        }else{
                            b_loc=(buffer[i]=='\r');
                        }
                        if (b_loc == 11){
                            b_urlfull = "";
                        }
                    }else if (b_loc == 11){
                        if (b_len == 0){
		            if (buffer[i]!=' '){
			       	b_urlfull += buffer[i];
			    }
                        }else{
                            assert(b_urlfull.size()>0);
                            b_loc++;
                        }
                    }
                }
                if (error > 0 && b_len<4 && b_idx<2){
                    state = 2;
                    break;
                }
                if (error > i){
		    error-=i;
                    memmove(buffer, buffer+i, error);
		    b_state = 4;
		    return error;
                }
                break;
            case 4:
                error = b_connect.breceive(buffer, size);
                return error;
        }
    }
    return error;
}
