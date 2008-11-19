#include <stdio.h>
#include <unistd.h>
#include "dhtloader.h"

int
main(int argc, char *argv[])
{
    dht_load(argc, argv);
    for (;;){
        dht_poll(true);
        sleep(1);
    }
    return 0;
}
