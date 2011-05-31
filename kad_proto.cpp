#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "kad_proto.h"

static char _curr_ident[21] = {
	"abcdefghij0123456789"
};

static uint8_t __ping_node_template[] = {
    "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t4:PPPP1:y1:qe"
};

static uint8_t __find_node_template[] = {
    "d1:ad2:id20:abcdefghij01234567896:target"
        "20:abcdefghij0123456789e1:q9:find_node1:t4:FFFF1:y1:qe"
};

static uint8_t __get_peers_template[] =  {
    "d1:ad2:id20:abcdefghij01234567899:info_hash"
        "20:mnopqrstuvwxyz123456e1:q9:get_peers1:t4:GGGG1:y1:qe"
};

static uint8_t __find_node_answer[] = {
   	"d1:rd2:id20:abcdefghij01234567895:nodes0:e1:t3:xxl1:y1:re"
};

int kad_set_ident(const uint8_t *ident)
{
	uint8_t *template0;

	template0 = __ping_node_template;
	memcpy(template0 + 12, ident, 20);

	template0 = __find_node_template;
	memcpy(template0 + 12, ident, 20);

	template0 = __get_peers_template;
	memcpy(template0 + 12, ident, 20);

	template0 = __find_node_answer;
	memcpy(template0 + 12, ident, 20);

	memcpy(_curr_ident, ident, 20);
	return 0;
}

int kad_get_ident(char *ident)
{
	memcpy(ident, _curr_ident, 20);
	return 0;
}

int kad_get_peers(void *buf, size_t len, uint32_t tid, const uint8_t *ident)
{
	size_t count;
	char *outp = (char *)buf;
	count = sizeof(__get_peers_template);
	assert(len > count);

	memcpy(outp, __get_peers_template, count);
	memcpy(outp + 46, ident, 20);
	memcpy(outp + 86, &tid, sizeof(tid));
	return (count - 1);
}

int kad_find_node(void *buf, size_t len, uint32_t tid, const uint8_t *ident)
{
	size_t count;
	char *outp = (char *)buf;
	count = sizeof(__find_node_template);
	assert(len > count);

	memcpy(outp, __find_node_template, count);
	memcpy(outp + 43, ident, 20);
	memcpy(outp + 83, &tid, sizeof(tid));
	return (count - 1);
}

int kad_ping_node(void *buf, size_t len, uint32_t tid)
{
	size_t count;
	char *outp = (char *)buf;
	count = sizeof(__ping_node_template);
	assert(len > count);

	memcpy(outp, __ping_node_template, count);
	memcpy(outp + 47, &tid, sizeof(tid));
	return (count - 1);
}

int kad_find_node_answer(void *buf, size_t len, const char *qid, size_t lid)
{
	char * outp;
	assert (45 + lid + 7 <= len);

	outp = (char *)buf;
	memcpy(outp, __find_node_answer, 45);
	memcpy(outp + 45, qid, lid);
	memcpy(outp + 45 + lid, __find_node_answer + 50, 7);

	return 45 + lid + 7;
}

int kad_less_than(const char *sp, const char *lp, const char *rp)
{
	int i;
	uint8_t l, r;

	i = 0;
	while (*lp == *rp) {
		if (i++ < 20) {
		   	lp++, rp++, sp++;
			continue;
		}
		break;
	}

	if (i < 20) {
	   	l = (*sp ^ *lp) & 0xFF;
	   	r = (*sp ^ *rp) & 0xFF;
	   	return (l < r);
	}

	return 0;
}

