#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "btcodec.h"
#include "kad_proto.h"

static char _last_token[21];
static char _curr_token[21] = {
	"abcdefghij0123456789"
};

static char _curr_ident[21] = {
	"abcdefghij0123456789"
};

int kad_set_ident(const void *ident)
{
	memcpy(_curr_ident, ident, 20);
	return 0;
}

int kad_get_ident(void *ident)
{
	memcpy(ident, _curr_ident, 20);
	return 0;
}

int kad_get_peers(void *buf, size_t len, uint32_t tid, const uint8_t *ident)
{
	btcodec codec;
	btentity *entity;
	btfastvisit btfv;
   	const char __template[] =  {
	   	"d1:ad2:id2:!!9:info_hash2:!!e1:q9:get_peers1:t2:!!1:y1:qe"
   	};

	codec.load(__template, sizeof(__template));
	entity = codec.str(&tid, sizeof(tid));
	btfv(&codec).bget("t").replace(entity);
	entity = codec.str(_curr_ident, 20);
	btfv(&codec).bget("a").bget("id").replace(entity);
	entity = codec.str(ident, 20);
	btfv(&codec).bget("a").bget("info_hash").replace(entity);

	return codec.encode(buf, len);
}

int kad_find_node(void *buf, size_t len, uint32_t tid, const uint8_t *ident)
{
   	btcodec codec;
	btentity *entity;
	btfastvisit btfv;
	const char __template[] = {
	   	"d1:ad2:id2:!!6:target2:!!e1:q9:find_node1:t2:!!1:y1:qe"
   	};

	codec.load(__template, sizeof(__template));
	entity = codec.str(&tid, sizeof(tid));
	btfv(&codec).bget("t").replace(entity);
	entity = codec.str(_curr_ident, 20);
	btfv(&codec).bget("a").bget("id").replace(entity);
	entity = codec.str(ident, 20);
	btfv(&codec).bget("a").bget("target").replace(entity);

	return codec.encode(buf, len);
}

int kad_ping_node(void *buf, size_t len, uint32_t tid)
{
	btcodec codec;
	btentity *entity;
	btfastvisit btfv;
	const char __template[] = {
	   	"d1:ad2:id2:!!e1:q4:ping1:t2:!!1:y1:qe"
   	};

	codec.load(__template, sizeof(__template));
	entity = codec.str(&tid, sizeof(tid));
	btfv(&codec).bget("t").replace(entity);
	entity = codec.str(_curr_ident, 20);
	btfv(&codec).bget("a").bget("id").replace(entity);

	return codec.encode(buf, len);
}

int kad_ping_node_answer(void *buf, size_t len, btentity *tid)
{
	btcodec codec;
	btentity *entity;
	btfastvisit btfv;
	const char __answer[] = {
	   	"d1:rd2:id2:!!e1:t3:!!!1:y1:re"
   	};

	codec.load(__answer, sizeof(__answer));
	btfv(&codec).bget("t").replace(tid);
	entity = codec.str(_curr_ident, 20);
	btfv(&codec).bget("r").bget("id").replace(entity);

	return codec.encode(buf, len);
}

int kad_find_node_answer(void *buf, size_t len, btentity *tid, const char *inp, size_t inl)
{
	btcodec codec;
	btentity *entity;
	btfastvisit btfv;
   	const char __answer[] = {
	   	"d1:rd2:id2:!!5:nodes2:!!e1:t3:!!!1:y1:re"
   	};

	codec.load(__answer, sizeof(__answer));
	btfv(&codec).bget("t").replace(tid);
	entity = codec.str(inp, inl);
	btfv(&codec).bget("r").bget("nodes").replace(entity);
	entity = codec.str(_curr_ident, 20);
	btfv(&codec).bget("r").bget("id").replace(entity);

	return codec.encode(buf, len);
}

int kad_get_peers_answer(void *buf, size_t len, btentity *tid, const char *inp, size_t inl)
{
	btcodec codec;
	btentity *entity;
	btfastvisit btfv;
   	const char __answer[] = {
		"d1:rd2:id2:!!5:token2:!!5:nodes2:!!e1:t3:!!!1:y1:re"
   	};

	codec.load(__answer, sizeof(__answer));
	btfv(&codec).bget("t").replace(tid);
	entity = codec.str(inp, inl);
	btfv(&codec).bget("r").bget("nodes").replace(entity);
	entity = codec.str(_curr_token, 20);
	btfv(&codec).bget("r").bget("token").replace(entity);
	entity = codec.str(_curr_ident, 20);
	btfv(&codec).bget("r").bget("id").replace(entity);

	return codec.encode(buf, len);
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

