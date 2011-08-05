#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <winsock.h>

#include "btcodec.h"
#include "kad_proto.h"
#include "kad_route.h"

static char _swptoken[21] = {
	"abcdefghij0123456789"
};

static char _curtoken[21] = {
	"abcdefghij0123456789"
};

int kad_get_token(const char **token)
{
	*token = _curtoken;
	return 0;
}

int kad_get_peers(void *buf, size_t len, uint32_t pid, const uint8_t *info_hash)
{
	uint8_t tid;
	btcodec codec;
	btentity *entity;
	btfastvisit btfv;
	const char *ident;

	const char example[] =  {
		"d1:ad2:id2:!!9:info_hash2:!!e1:q9:get_peers1:t2:!!1:y1:qe"
	};

	tid = pid;
	kad_get_ident(&ident);
	codec.load(example, sizeof(example));
	entity = codec.str(&tid, sizeof(tid));
	btfv(&codec).bget("t").replace(entity);
	entity = codec.str(ident, IDENT_LEN);
	btfv(&codec).bget("a").bget("id").replace(entity);
	entity = codec.str(info_hash, HASH_LEN);
	btfv(&codec).bget("a").bget("info_hash").replace(entity);

	return codec.encode(buf, len);
}

int kad_find_node(void *buf, size_t len, uint32_t pid, const uint8_t *target)
{
	uint8_t tid;
	btcodec codec;
	btentity *entity;
	btfastvisit btfv;
	const char *ident;

	const char example[] =  {
		"d1:ad2:id2:!!6:target2:!!e1:q9:find_node1:t2:!!1:y1:qe"
	};

	tid = pid;
	kad_get_ident(&ident);
	codec.load(example, sizeof(example));
	entity = codec.str(&tid, sizeof(tid));
	btfv(&codec).bget("t").replace(entity);
	entity = codec.str(ident, IDENT_LEN);
	btfv(&codec).bget("a").bget("id").replace(entity);
	entity = codec.str(target, IDENT_LEN);
	btfv(&codec).bget("a").bget("target").replace(entity);

	return codec.encode(buf, len);
}

int kad_ping_node(void *buf, size_t len, uint32_t pid)
{
	uint8_t tid;
	btcodec codec;
	btentity *entity;
	btfastvisit btfv;
	const char *ident;

	const char example[] = {
		"d1:ad2:id2:!!e1:q4:ping1:t2:!!1:y1:qe"
	};

	tid = pid;
	kad_get_ident(&ident);
	codec.load(example, sizeof(example));
	entity = codec.str(&tid, sizeof(tid));
	btfv(&codec).bget("t").replace(entity);
	entity = codec.str(ident, IDENT_LEN);
	btfv(&codec).bget("a").bget("id").replace(entity);

	return codec.encode(buf, len);
}

int kad_ping_node_answer(void *buf, size_t len, btentity *pid)
{
	btcodec codec;
	btentity *entity;
	btfastvisit btfv;
	const char *ident;

	const char answer[] = {
		"d1:rd2:id2:!!e1:t3:!!!1:y1:re"
	};

	kad_get_ident(&ident);
	codec.load(answer, sizeof(answer));
	btfv(&codec).bget("t").replace(pid);
	entity = codec.str(ident, IDENT_LEN);
	btfv(&codec).bget("r").bget("id").replace(entity);

	return codec.encode(buf, len);
}

int kad_find_node_answer(void *buf, size_t len,
		btentity *pid, const char *inp, size_t inl)
{
	btcodec codec;
	btentity *entity;
	btfastvisit btfv;
	const char *ident, *token;

	const char answer[] = {
		"d1:rd2:id2:!!5:nodes2:!!e1:t3:!!!1:y1:re"
	};

	kad_get_ident(&ident);
	codec.load(answer, sizeof(answer));
	btfv(&codec).bget("t").replace(pid);
	entity = codec.str(inp, inl);
	btfv(&codec).bget("r").bget("nodes").replace(entity);
	entity = codec.str(ident, IDENT_LEN);
	btfv(&codec).bget("r").bget("id").replace(entity);

	return codec.encode(buf, len);
}

int kad_get_peers_answer(void *buf, size_t len,
		btentity *pid, const char *inp, size_t inl)
{
	btcodec codec;
	btentity *entity;
	btfastvisit btfv;
	const char *ident, *token;

	const char answer[] = {
		"d1:rd2:id2:!!5:token2:!!5:nodes2:!!e1:t3:!!!1:y1:re"
	};

	kad_get_ident(&ident);
	kad_get_token(&token);
	codec.load(answer, sizeof(answer));
	btfv(&codec).bget("t").replace(pid);
	entity = codec.str(inp, inl);
	btfv(&codec).bget("r").bget("nodes").replace(entity);
	entity = codec.str(ident, IDENT_LEN);
	btfv(&codec).bget("r").bget("id").replace(entity);
	entity = codec.str(token, TOKEN_LEN);
	btfv(&codec).bget("r").bget("token").replace(entity);

	return codec.encode(buf, len);
}

int kad_error_node_answer(void *buf, size_t len, btentity *pid)
{
	btcodec codec;
	btentity *entity;
	btfastvisit btfv;
	const char *ident;

	const char example[] = {
		"d1:t3:!!!1:y1:e1:eli201e5:erroree"
	};

	codec.load(example, sizeof(example));
	entity = codec.str(&pid, sizeof(pid));
	btfv(&codec).bget("t").replace(entity);
	return codec.encode(buf, len);
}

int kad_less_than(const char *bp, const char *lp, const char *rp)
{
	int i;
	uint8_t l, r;

	i = 0;
	while (*lp == *rp) {
		if (i++ < 20) {
			lp++, rp++, bp++;
			continue;
		}
		break;
	}

	if (i < 20) {
		l = (*bp ^ *lp) & 0xFF;
		r = (*bp ^ *rp) & 0xFF;
		return (l < r);
	}

	return 0;
}

