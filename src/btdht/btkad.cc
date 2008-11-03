#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <map>

#include "btkad.h"
#include "bthread.h"
#define  _K 8
#define CONCURRENT_REQUEST _K

static std::map<bthread*, std::string> __find_node_sleep_queue;

int _find_node(char target[20])
{
    int i;
    int concurrency = 0;
    knode nodes[_K];
    int count = get_knode(target, nodes, _K, false);
    if (count == -1){
        return -1;
    }
#if 0
    if (valid_count(nodes, _K) < 4){
        count = get_knode(nodes, _K, false);
    }
#endif
    int rds[CONCURRENT_REQUEST];
    while (not finish){
        i = 0;
        while (concurrency<CONCURRENT_REQUEST && i<_K){
            if (nodes[i].requested()){
                continue;
            }
            int rd = nodes[i]->find_node(target);
            if (rd != -1){
                rds[concurrency++] = rd;
            }
        }
        wait_for_result(rds, concurrent, 5);
        int end = 0;
        for (i=0; i<concurrency; i++){
            if (find_finish(rds[i])){
                continue;
            }
            if (end < i){
                rds[end] = rds[i];
            }
            end++;
        }
    }
    return 0;
}

int
btkad::find_node(char target[20])
{
    bthread *curthread = bthread::now_job();

    if (__find_node_sleep_queue.find(curthread)
            == __find_node_sleep_queue.end()){
        int count = get_knodes(target);
        __find_node_sleep_queue.insert(
                std::make_pair(curthread, target));
        curthread->tsleep();
        return -1;
    }
    __find_node_sleep_queue.erase(curthread);
    return 0;
}
