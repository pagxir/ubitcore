/* $Id$ */
#include <string.h>
#include <assert.h>

#include "btracker.h"
#include "bpeermgr.h"

burlthread::burlthread(const char *url, int second):
    b_url(url)
{
    b_state = 0;
    b_second = second;
    last_time = time(NULL);

    int i = 0;
    const char *dot = url;
    while (*dot && *dot!='?'){
        b_path[i++] = *dot;
        if (*dot == '/'){
            i = 0;
        }
        dot++;
    }
    if (i == 0){
        strcpy(b_path, "a.out");
    }else{
        b_path[i] = 0;
    }
}


int
burlthread::bdocall(time_t timeout)
{
    int error = 0;
    int state = b_state;
    char buffer[81920];
    const char *burl = NULL;
    while(error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                b_get.reset(burlget::get());
                b_data.clear();
                error = b_get->burlbind(b_url.c_str());
                break;
            case 1:
                error = b_get->bpolldata(buffer, sizeof(buffer));
                break;
            case 2:
                if (error > 0){
                    b_data.append(buffer, error);
                    state = 1;
                    break;
                }
                break;
            case 3:
                burl = b_get->blocation();
                if (burl!=NULL && b_data.empty()){
                    assert(strncmp(burl, "http://", 7)==0);
                    burlget *get = burlget::get();
                    get->burlbind(burl);
                    b_get.reset(get);
                    state = 6;
                }
                break;
            case 4:
                if (!b_data.empty()){
                    bload_peer(b_data.c_str(), b_data.size());
                }
                assert(error==0);
                error = btime_wait(last_time+b_second);
                break;
            case 5:
                last_time = time(NULL);
                state = 0;
                break;
            case 6:
                error = b_get->bpolldata(buffer, sizeof(buffer));
                break;
            case 7:
                if (error > 0){
                    b_data.append(buffer, error);
                    state = 6;
                    break;
                }
                break;
            case 8:
                state = 4;
                break;
            default:
                assert(0);
                break;
        }
    }
    return error;
}

burlthread::~burlthread()
{

}
