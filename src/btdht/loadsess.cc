#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string>

#include "btkad.h"
#include "kutils.h"
#include "ktable.h"
#include "btcodec.h"


int load_session(const char *path)
{
    char buffer[1024*32];
    genkadid(buffer);
    setkadid(buffer);
    FILE *fp = fopen(path, "rb");
    if (fp == NULL){
        return 0;
    }
    int n = fread(buffer, 1, sizeof(buffer), fp);
    fclose(fp);
    size_t len;
    btcodec codec;
    codec.bload(buffer, n);
    const char *fileguard = codec.bget().bget(".fileguard").c_str(&len);
    if (fileguard != NULL){
        printf(".fileguard: %s\n", std::string(fileguard, len).c_str());
    }
    int64_t val;
    int age = codec.bget().bget("age").bget(&val);
    if (val != -1){
        time_t now = age;
        printf("age: %s\n", ctime(&now));
    }
    const char *id = codec.bget().bget("id").c_str(&len);
    if (id != NULL){
        printf("ident: %s\n", idstr(id));
        setkadid(id);
    }
    kitem_t item;
    typedef char compat_t[26];
    const char *nodes = codec.bget().bget("nodes").c_str(&len);
    if (nodes != NULL){
        compat_t *iter, *end = (compat_t*)(nodes+len);
        for (iter=(compat_t*)nodes; iter<end; iter++){
            memcpy(item.kadid, &(*iter)[0], 20);
            item.host = *(in_addr_t*)&(*iter)[20];
            item.port = *(in_port_t*)&(*iter)[24];
            update_contact(&item, false);
        }
        assert((len%26) == 0);
    }
    return 0;
}
