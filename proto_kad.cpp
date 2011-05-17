#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "proto_kad.h"

uint8_t __ping_node_template[] = {
    "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t4:PPPP1:y1:qe"
};

uint8_t __find_node_template[] = {
    "d1:ad2:id20:abcdefghij01234567896:target"
        "20:abcdefghij0123456789e1:q9:find_node1:t4:FFFF1:y1:qe"
};

uint8_t __get_peers_template[] =  {
    "d1:ad2:id20:abcdefghij01234567899:info_hash"
        "20:mnopqrstuvwxyz123456e1:q9:get_peers1:t4:GGGG1:y1:qe"
};

int kad_get_peers(void * buf, size_t len, uint32_t tid, uint8_t ident[20])
{
	size_t count;
	char * outp = (char *)buf;
	count = sizeof(__get_peers_template);
	assert(len > count);

	memcpy(outp, __get_peers_template, count);
	memcpy(outp + 46, ident, 20);
	memcpy(outp + 86, &tid, sizeof(tid));
	return (count - 1);
}

int kad_find_node(void * buf, size_t len, uint32_t tid, uint8_t ident[20])
{
	size_t count;
	char * outp = (char *)buf;
	count = sizeof(__find_node_template);
	assert(len > count);

	memcpy(outp, __find_node_template, count);
	memcpy(outp + 43, ident, 20);
	memcpy(outp + 83, &tid, sizeof(tid));
	return (count - 1);
}

int kad_ping_node(void * buf, size_t len, uint32_t tid)
{
	size_t count;
	char * outp = (char *)buf;
	count = sizeof(__find_node_template);
	assert(len > count);

	memcpy(outp, __find_node_template, count);
	memcpy(outp + 47, &tid, sizeof(tid));
	return (count - 1);
}

