#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <map>

#include "btkad.h"
#include "knode.h"
#include "kbucket.h"
#include "ktable.h"
#include "bthread.h"
#include "bsocket.h"
#include "btcodec.h"
#include "transfer.h"
#include "provider.h"
#include "btimerd.h"
#include "kgetpeers.h"
#include "services.h"
#include "kfind.h"


int accept_request(bsocket *si);

serviced::serviced()
{
    b_state = 0;
    b_ident = "serviced";
    b_start_time = now_time();
}

int
serviced::bdocall()
{
    int state = b_state;

    int n;
    char buff[8192];

    kitem_t items[8];

    do{
        b_state = state++;
        switch(b_state){
            case 0:
                accept_request(&b_socket);
                break;
            case 1:
                n = b_socket.breceive(buff, sizeof(buff));
                break;
            case 2:
                printf("len: %d\n", n);
                if (n < 20){
                    state = 0;
                    break;
                }
                buff[n] = 0;
                break;
            case 3:
                n = find_nodes(buff, items, false);
                if (n == 0)
                    state = 0;
                break;
            case 4:
                assert(b_getpeers==NULL);
                b_getpeers = kgetpeers_new(buff, items, n);
                printf("get peers: %s\n", idstr(buff));
                break;
            case 5:
                b_getpeers->vcall();
                while (!b_getpeers->empty()){
                    char sbuff[6];
                    peer p = b_getpeers->front();
                    memcpy(sbuff, &p.addr, sizeof(p.addr));
                    memcpy(&sbuff[4], &p.port, sizeof(p.port));
                    if (-1 == b_socket.bsend(sbuff, sizeof(sbuff))){
                        delete b_getpeers;
                        b_getpeers = NULL;
                        b_state = state = 0;
                        b_runable = true;
                        break;
                    }
                    b_getpeers->pop();
                }
                break;
            case 6:
                delete b_getpeers;
                b_getpeers = NULL;
                break;
            case 7:
                state = 0;
                break;
            default:
                break;
        }
    }while(b_runable == true);
    return 0;
}
