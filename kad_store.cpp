#include <stdio.h>
#include <windows.h>
#include <assert.h>

struct info_peer {
	int t_tick;
	int t_len;
	char t_value[20];
	struct info_peer *t_next;
};

struct info_hash {
	int t_tick;
	char t_hash[20];
	struct info_hash *t_next;
	struct info_peer *t_peers;
};

static struct info_hash *_info_hash_list = 0;
static int now(void)
{
	return GetTickCount() / 1000;
}

static int update_peer(struct info_hash *hash, const char *value, size_t len)
{
	struct info_peer *peer = hash->t_peers;

	while (peer != NULL) {
		if (len == peer->t_len &&
			 !memcmp(peer->t_value, value, len))
			break;
		peer = peer->t_next;
	}

	if (peer == NULL) {
		peer = (struct info_peer *)malloc(sizeof(info_peer));
		assert(peer != NULL);
		peer->t_next = hash->t_peers;
		hash->t_peers = peer;
	}

	memcpy(peer->t_value, value, len);
	peer->t_len = len;
	hash->t_tick = now();
	return 0;
}

int announce_value(const char *info_hash, const char *value, size_t len)
{
	struct info_hash *hash;
	
	hash = _info_hash_list;
	while (hash != NULL) {
		if (!memcmp(hash->t_hash, info_hash, 20))
			break;
		hash = hash->t_next;
	}

	if (hash == NULL) {
		hash = (struct info_hash *)malloc(sizeof(struct info_hash));
		assert(hash != NULL);
		memcpy(hash->t_hash, info_hash, 20);
		hash->t_peers = NULL;
		hash->t_next = _info_hash_list;
		_info_hash_list = hash;
	}

	update_peer(hash, value, len);
	return 0;
}

int kad_get_values(const char *info_hash, char *value, size_t len)
{
	char *p;
	struct info_hash *hash;
	struct info_peer *peer;
	
	hash = _info_hash_list;
	while (hash != NULL) {
		if (!memcmp(hash->t_hash, info_hash, 20))
			break;
		hash = hash->t_next;
	}

	p = value;
	if (hash != NULL) {
		*p++ = 'l';
		peer = hash->t_peers;
		 while (peer != NULL) {
			if (peer->t_len > 0 && peer->t_tick + 15 * 60 > now()) {
				p += sprintf(p, "%d:", peer->t_len);
				memcpy(p, peer->t_value, peer->t_len);
				p += peer->t_len;
			}
			peer = peer->t_next;
		}
		*p++ = 'e';
		return (p - value);
	}

	return 0;
}

int store_value_dump(void)
{
	struct info_peer *peer;
	struct info_hash *hash;

	hash = _info_hash_list;
	while (hash != NULL) {
		for (int i = 0; i < 20; i++)
			printf("%02X", hash->t_hash[i] & 0xFF);
		printf("\n");
		peer = hash->t_peers;
		 while (peer != NULL) {
			unsigned char *value = (unsigned char *)peer->t_value;
			printf("%d.%d.%d.%d:%d\n",
				value[0], value[1], value[2], value[3], (value[4] << 8) + value[5]);
			peer = peer->t_next;
		}
		hash = hash->t_next;
	}
	return 0;
}

