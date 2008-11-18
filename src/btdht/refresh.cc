#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <map>

#include "btkad.h"
#include "refresh.h"
#include "kfind.h"
#include "knode.h"
#include "btimerd.h"



refreshd::refreshd(int index)
{
    b_find = NULL;
    b_state = 0;
    b_retry = 0;
    b_index = index;
    b_ident = "refreshd";
    b_usevalid = false;
    b_last_update = now_time();
}

int
refreshd::bdocall()
{
    char bootid[20];
    size_t count;
    kitem_t items[8];
    int error = 0;
    int state = b_state;
    while (b_runable){
        b_state = state++;
        switch(b_state)
        {
            case 0:
                genrefreshid(bootid, b_index);
                count = find_nodes(bootid, items, b_usevalid);
                if (count == 0){
                    count = find_nodes(bootid, items, false);
                }
                if (count == 0){
                    tsleep(NULL, "exit");
                    return 0;
                }
		b_count = count;
                memcpy(b_target, bootid, 20);
		memcpy(b_findNodes, items, count*sizeof(kitem_t));
                b_find = kfind_new(bootid, items, count);
                b_last_update = time(NULL);
                b_usevalid = false;
                break;
            case 1:
                error = b_find->vcall();
                break;
            case 2:
                if (error<3 && b_retry<3){
                    b_usevalid = true;
                    state = 0;
                }
		if (error == 0){
                    int i;
                    printf("refresh: ");
                    printf("%s\n", idstr(b_target));
                    for (i=0; i<b_count; i++){
                        printf("%s  %s:%d\n",
                                idstr(b_findNodes[i].kadid),
                                inet_ntoa(*(in_addr*)&b_findNodes[i].host),
                                htons(b_findNodes[i].port));
                    }
                    printf("re:%d@%d\n", b_retry, b_index);
		}
                break;
            case 3:
                if (b_last_update + 800 > time(NULL)){
                    tsleep(NULL, "exit");
                    b_retry = 0;
                }
                state = 0;
                break;
            default:
                assert(0);
                break;
        }
    }
    return error;
}
