#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <map>

#include "btkad.h"
#include "bthread.h"
#include "kbucket.h"
#include "kfind.h"
#include "knode.h"
#include "bsocket.h"
#include "btcodec.h"
#include "provider.h"
#include "transfer.h"

struct kfind_arg{
    char kadid[20];
    in_addr_t host;
    in_port_t port;
    time_t    age;
    time_t    atime;
    int       failed;
    kship     *ship;
};

kfind::kfind(bdhtnet *net, const char target[20])
{
    b_state = 0;
    b_net   = net;
    b_concurrency = 0;
    memcpy(b_target, target, 20);
}

int
kfind::vcall()
{
    int i;
    int count;
    int error = 0;
    int state = b_state;
    char buffer[8192];
    in_addr_t host;
    in_port_t port;
    std::vector<kfind_arg*>::iterator iter;
    kitem_t nodes[_K];

    while (error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                count = get_knode(b_target, nodes, false);
                if (count == -1){
                    return 0;
                }
                for (i=0; i<count; i++){
                    kfind_arg *arg = new kfind_arg;
                    arg->host = nodes[i].host;
                    arg->port = nodes[i].port;
                    memcpy(arg->kadid, nodes[i].kadid, 20);
                    kaddist_t dist(arg->kadid, b_target);
                    b_kfind_queue.insert(
                            std::make_pair(dist, arg));
                }
                break;
            case 1:
                while (b_concurrency<CONCURRENT_REQUEST){
                    if (b_kfind_queue.empty()){
                        break;
                    }
                    kfind_arg *arg = b_kfind_queue.begin()->second;
                    arg->ship = b_net->get_kship();
                    arg->ship->find_node(arg->host, arg->port,
                            (uint8_t*)b_target);
                    b_concurrency++;
                }
                break;
            case 2:
                for (iter = b_kfind_out.begin(); 
                        iter != b_kfind_out.end();
                        iter++){
                    if ((*iter)->ship == NULL){
                        continue;
                    }
                    kship *ship = (*iter)->ship;
                    count = ship->get_response(buffer,
                                sizeof(buffer), &host, &port);
                    if (count > 0){
                        printf("Fould Packet!\n");
                        delete ship;
                        (*iter)->ship = NULL;
                        state = 1;
                    }
                }
                break;
            case 3:
                bthread::now_job()->tsleep(this, time(NULL)+34);
                error = -1;
                break;
            default:
                break;
        }
    }
    return 0;
}
