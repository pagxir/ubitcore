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
    kitem_t nodes[_K];

    while (error != -1){
        b_state = state++;
        switch(b_state){
            case 0:
                count = get_knode(b_target, nodes, false);
                if (count == -1){
                    return -1;
                }
                break;
            case 1:
                while (i<_K && b_concurrency<CONCURRENT_REQUEST){
                }
                break;
            case 2:
                break;
            default:
                break;
        }
    }
    return 0;
}
