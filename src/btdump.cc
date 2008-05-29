#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <queue>
#include <dht_throttle.h>
#include "sha1.h"

FILE *__static_file = NULL;
extern char __bitField[];
static const char *__config_name = NULL;
int segbuf_read(int piece, int start, char buf[], int length);
#define BTCHECK
static int bfload(const char *path, char buf[], int len)
{
    int count =  0;
    FILE *fp = fopen(path, "rb");
    if (fp != NULL) {
        count = fread(buf, 1, len, fp);
        fclose(fp);
    }
    return count;
}

static FILE *cacheopen(const char *path, const char *mode)
{
	FILE *fp = fopen(path, mode);
	if (fp != NULL){
		return fp;
	}
	int i;
	int count = 0;
	static char buf[4096];
	for (i=0; path[i]; i++){
		buf[count++] =  path[i];
		if (path[i]=='/'){
			buf[count] = 0;
			mkdir(buf, 0777);
		}
	}
	return fopen(path, mode);
}

static const char *btcheck(const char *btbuf, const char *btend);

static const char *btcheck_list(const char *btbuf, const char *btend)
{
    const char *btch = btbuf;
    if (btch<btend && *btch=='l') {
        btch++;
        while (btch<btend && *btch != 'e') {
            btch = btcheck(btch, btend);
        }
        return btch+1;
    }
    return btend+1;
}

static const char *btcheck_int(const char *btbuf, const char *btend)
{
    int count = 0;
    const char *btch = btbuf;
    if (btch<btend && *btch=='i') {
        btch++;
        while (btch<btend && *btch!='e') {
            count *= 10;
            count += (*btch-'0');
            btch++;
        }
        return btch+1;
    }
    return btend+1;
}

static const char *btcheck_str(const char *btbuf, const char *btend)
{
    int count = 0;
    const char *btch = btbuf;
    while (btch<btend && isdigit(*btch)) {
        count *= 10;
        count += (*btch-'0');
        btch++;
    }
    if (btch<btend && *btch!=':') {
        return btend+1;
    }
    btbuf = btch + count + 1;
    return btbuf;
}

static const char *btcheck_dict(const char *btbuf, const char *btend)
{
    const char *btch = btbuf;
    if (btch<btend && *btch=='d') {
        btch++;
        while (btch<btend && *btch!='e') {
            btch = btcheck_str(btch, btend);
            btch = btcheck(btch, btend);
        }
        return btch+1;
    }
    return btend+1;
}

static const char *btcheck(const char *btbuf, const char *btend)
{
    if (btbuf<btend) {
        switch (*btbuf) {
        case 'd':
            return btcheck_dict(btbuf, btend);
        case 'l':
            return btcheck_list(btbuf, btend);
        case 'i':
            return btcheck_int(btbuf, btend);
        default:
            if (isdigit(*btbuf)) {
                return btcheck_str(btbuf, btend);
            }
            break;
        }
    }
    return btend+1;
}
#undef BTCHECK

int btload_int(const char *btbuf, const char *btend)
{
    int count = 0;
    if (btbuf<btend && *btbuf++!='i') {
        return -1;
    }
    while (btbuf<btend && isdigit(*btbuf)) {
        count *= 10;
        count += (*btbuf-'0');
        btbuf++;
    }
    if (btbuf<btend && *btbuf=='e') {
        return count;
    }
    return -1;
}

const char *btload_str(const char *btbuf,
                              const char *btend, int *plen)
{
    int count = 0;
    while (btbuf<btend && isdigit(*btbuf)) {
        count *= 10;
        count += (*btbuf-'0');
        btbuf++;
    }
    if (btbuf<btend && *btbuf==':') {
        *plen = count;
        return btbuf+1;
    }
    return btend+1;
}

const char *btstr_dup(const char *btstr, const char *btend)
{
    int len;
    const char *str = btload_str(btstr, btend, &len);
    if (str < btend) {
        char *dp = new char[len+1];
        memcpy(dp, str, len);
        dp[len] = 0;
        return dp;
    }
    return NULL;
}

const char *btload_list(const char *btbuf,
                               const char *btend, int index)
{
    int count = 0;
    const char *btch = btbuf;
    if (btbuf<btend && *btch++=='l') {
        while (*btch != 'e') {
            if (count == index) {
                return btch;
            }
            count++;
            btch = btcheck(btch, btend);
        }
    }
    return btend+1;
}

const char *btload_dict(const char *btbuf,
                               const char *btend, const char *name)
{
    if (btbuf<btend && *btbuf++=='d') {
        while (*btbuf != 'e') {
            if (0==strncmp(name, btbuf, strlen(name))) {
                return btbuf+strlen(name);
            }
            btbuf = btcheck_str(btbuf, btend);
            btbuf = btcheck(btbuf, btend);
        }
    }
    return btend+1;
}

int tfdump_info_hash(const char *buf, int count)
{
    const char *btend = buf+count;
    const char *info = btload_dict(buf, btend, "4:info");
    if (info > btend) {
        return 0;
    }
    const char *info_end = btcheck_dict(info, btend);
    if (info_end > btend) {
        return 0;
    }
    int i;
    unsigned char digest[20];
    SHA1Hash(digest, (unsigned char*)info, info_end-info);
    for (i=0; i<20; i++) {
        printf("%02x", digest[i]);
    }
    printf("\n");
    return 0;
}

static const char *tfdump_path(const char *path, const char *btend, const char *base)
{
    int i = 0;
    int len = 0;
    int count = 0;
    char *fpath = NULL;
    const char *name = btload_list(path, btend, i++);
    const char *name_end = btcheck_list(path, btend);
    if (name_end > name) {
        count = name_end - name;
        fpath = new char[count+strlen(base)];
        len = sprintf(fpath, "%s", base);
    }
    while (name<btend) {
        const char *dup = btstr_dup(name, btend);
        strcat(fpath, "/");
        strcat(fpath, dup);
        delete[] dup;
        name = btload_list(path, btend, i++);
    }
    return fpath;
}

static int tfdump_file(const char *file, const char *btend, const char *base)
{
    const char *length = btload_dict(file, btend, "6:length");
    if (length>btend) {
        return 0;
    }
    int len_of_file = btload_int(length, btend);
    const char *path = btload_dict(file, btend, "10:path.utf-8");
    const char *path_name = tfdump_path(path, btend, base);
    printf("[%d]%s\n", len_of_file, path_name);
    delete[] path_name;
    return 0;
}

static int tfdump_files(const char *btbuf, const char *btend, const char *base)
{
    int i = 0;
    const char *file = btload_list(btbuf, btend, i++);
    while (file<btend) {
        tfdump_file(file, btend, base);
        file = btload_list(btbuf, btend, i++);
    }
    return 0;
}

static int tfdump_info_file_list(const char *buf, int count)
{
    const char *btend = buf+count;
    const char *info = btload_dict(buf, btend, "4:info");
    if (info > btend) {
        return 0;
    }
    const char *info_end = btcheck_dict(info, btend);
    if (info_end > btend) {
        return 0;
    }
    const char *name = btload_dict(info, btend, "10:name.utf-8");
    if (name > btend) {
        return 0;
    }
    const char *folder = btstr_dup(name, btend);
    printf("name: %s\n", folder);
    const char *files = btload_dict(info, btend, "5:files");
    const char *length = btload_dict(info, btend, "6:length");
    if (length<=btend && files>btend) {
        printf("length: %d\n", btload_int(length, btend));
        return 0;
    }
    if (files<=btend&&length>btend) {
        tfdump_files(files, btend, folder);
        return 0;
    }
    delete[] folder;
    printf("fotal error: %p %p %p\n", length, files, btend);
    return 0;
}

static int tfdump_announce(const char *btbuf, int count)
{
    const char *btend = btbuf+count;
    const char *announce = btload_dict(btbuf, btend, "8:announce");
    if (announce < btend) {
        const char *ap = btstr_dup(announce, btend);
        printf("announce: %s\n", ap);
        delete[] ap;
    }
    return 0;
}

static int tfdump_announce(const char *btbuf, const char *btend)
{
    int i = 0;
    const char *announce = btload_list(btbuf, btend, i++);
    while (announce < btend) {
        const char *ap = btstr_dup(announce, btend);
        printf("announce[list]: %s\n", ap);
        delete[] ap;
        announce = btload_list(btbuf, btend, i++);
    }
    return 0;
}

static int tfdump_announce_list(const char *btbuf, int count)
{
    const char *btend = btbuf+count;
    const char *announce_list = btload_dict(btbuf, btend, "13:announce-list");
    if (announce_list < btend) {
        int i = 0;
        const char *announce = btload_list(announce_list, btend, i++);
        while (announce<btend) {
            tfdump_announce(announce, btend);
            announce = btload_list(announce_list, btend, i++);
        }
    }
    return 0;
}

static int tfdump_creation_date(const char *btbuf, int count)
{
    const char *btend = btbuf+count;
    const char *announce = btload_dict(btbuf, btend, "13:creation date");
    if (announce < btend) {
        int date = btload_int(announce, btend);
        printf("creation date: %d\n", date);
    }
    return 0;
}

static int tfdump_comment(const char *btbuf, int count)
{
    const char *btend = btbuf+count;
    const char *announce = btload_dict(btbuf, btend, "7:comment");
    if (announce < btend) {
        const char *ap = btstr_dup(announce, btend);
        //printf("comment: %s\n", dp);
        delete[] ap;
    }
    return 0;
}

static int tfdump_create_by(const char *btbuf, int count)
{
    const char *btend = btbuf+count;
    const char *announce = btload_dict(btbuf, btend, "10:created by");
    if (announce < btend) {
        const char *ap = btstr_dup(announce, btend);
        printf("created by: %s\n", ap);
        delete[] ap;
    }
    return 0;
}

static int tfdump_node_element(const char *btbuf, const char *btend)
{
    int port0;
    const char *ip = btload_list(btbuf, btend, 0);
    const char *port = btload_list(btbuf, btend, 1);
    if (port < btend) {
        port0 = btload_int(port, btend);
    }
    if (ip < btend) {
        const char *ap = btstr_dup(ip, btend);
        printf("node %s:%d\n", ap, port0);
        delete[] ap;
    }
    return 0;
}

static int tfdump_nodes(const char *btbuf, int count)
{
    const char *btend = btbuf+count;
    const char *node_list = btload_dict(btbuf, btend, "5:nodes");
    if (node_list < btend) {
        int i = 0;
        const char *node = btload_list(node_list, btend, i++);
        while (node<btend) {
            tfdump_node_element(node, btend);
            node = btload_list(node_list, btend, i++);
        }
    }
    return 0;
}

#ifdef EXEC
static char __static__buffer[2*1024*1024];
int main(int argc, char *argv[])
{
    int i;
    int count;
    for (i=1; i<argc; i++) {
        count = bfload(argv[i], __static__buffer, sizeof(__static__buffer));
        if (btcheck(__static__buffer, __static__buffer+count)
                <=__static__buffer+count) {
            tfdump_announce(__static__buffer, count);
            tfdump_announce_list(__static__buffer, count);
            tfdump_creation_date(__static__buffer, count);
            tfdump_comment(__static__buffer, count);
            tfdump_create_by(__static__buffer, count);
            tfdump_info_file_list(__static__buffer, count);
            tfdump_nodes(__static__buffer, count);
            tfdump_info_hash(__static__buffer, count);
        }
    }
    return 0;
}
#else

int __piece_size = 0;
int __piece_count = 0;
int __last_piece_size  = 0;

static char __peer_text_id[512];
const char *get_peer_id()
{
    return __peer_text_id;
}

static char __info_text_hash[512];
const char *get_info_hash()
{
    return __info_text_hash;
}

static char __protocol[68] = {
    "\0BitTorrent protocol\0\0\0\0\0\0\0\0"
};
const char *get_bin_info_hash()
{
    return __protocol+28;
}

int get_hand_shake(char *buf, int len)
{
    if (len < 68) {
        return -1;
    }
    memcpy(buf, __protocol, 68);
    return 68;
}

static int update_session(unsigned char degist[20])
{
    int i;
    int rnd;
    __protocol[0] = 0x13;
    srand(time(NULL));
    strcpy(__protocol+48, "-UL0012-");
    for (i=0; i<20; i++) {
        int rnd = __protocol[48+i];
        rnd = (i>7?rand()&0xff:rnd);
        __protocol[28+i] = degist[i];
        __protocol[48+i] = rnd;
        sprintf(__info_text_hash+3*i, "%%%02x", degist[i]);
        sprintf(__peer_text_id+3*i, "%%%02x", 0xff&rnd);
    }
    return 0;
}

static std::queue<char*> __queue_announce;
int get_announce(char *announce, int len)
{
    if (!__queue_announce.empty()) {
        strcpy(announce, __queue_announce.front());
        free(__queue_announce.front());
        __queue_announce.pop();
        assert(len>strlen(announce));
        printf("tracker to: %s\n", announce);
        return strlen(announce);
    }
    return -1;
}

static int do_add_announce0(const char *btbuf, const char *btend)
{
    const char *url = btstr_dup(btbuf, btend);
    __queue_announce.push(strdup(url));
    delete[] url;
    return 0;
}

static int do_add_announce(const char *btbuf, const char *btend)
{
    const char *url = btload_list(btbuf, btend, 0);
    do_add_announce0(url, btend);
    return 0;
}

static int add_piece_length(int count)
{
    __last_piece_size += count;
    __piece_count += (__last_piece_size/__piece_size);
    __last_piece_size %= __piece_size;
    return 0;
}

struct _disk_file_t {
    int df_first_piece;
    int df_first_byte;
    int df_last_piece;
    int df_last_byte;
    FILE *df_cache_file;
    const char *df_local_path;
};


int __local_disk_file_count = 0;
_disk_file_t __local_disk_files[100];

static int save_local_file_path(_disk_file_t *dist_file)
{
    printf("loading file:\n");
    printf("\tpath: %s\n", dist_file->df_local_path);
    printf("\trange: %dx%d+%d - %dx%d+%d\n",
           dist_file->df_first_piece, __piece_size, dist_file->df_first_byte,
           dist_file->df_last_piece, __piece_size, dist_file->df_last_byte);
    if (__local_disk_file_count < 100) {
        __local_disk_files[__local_disk_file_count++]
        = *dist_file;
    }
    return 0;
}

static int load_file_path(const char *btbuf, const char *btend,
                          const char *base, _disk_file_t *dist_file)
{
    int i=0;
    const char *entry = btload_list(btbuf, btend, i++);
    const char *entry_end = btcheck_list(btbuf, btend);
    char *path = new char[strlen(base)+1+entry_end-entry];
    strcpy(path, base);
    while (entry < btend) {
        const char *title = btstr_dup(entry, btend);
        strcat(path, "/");
        strcat(path, title);
        delete[] title;
        entry = btload_list(btbuf, btend, i++);
    }
    dist_file->df_local_path = path;
    return 0;
}

static int load_file_length(const char *btbuf, const char *btend,
                            const char *base_path, _disk_file_t *dist_file)
{
    int i;
    int value = 0;
    int offset = 0;
    int base = 1000000000;
    const char *path_utf8 = btload_dict(btbuf, btend, "10:path.utf-8");
    const char *path = btload_dict(btbuf, btend, "4:path");
#ifdef UTF_8
    if (path_utf8 < btend) {
        path = path_utf8;
    }
#endif
    load_file_path(path, btend, base_path, dist_file);
    const char *length = btload_dict(btbuf, btend, "6:length");
    if (length >= btend) {
        return -1;
    }
    const char *length_end = btcheck_int(length, btend);
    if (length_end >= btend) {
        return -1;
    }
    if (length+12 > length_end) {
        add_piece_length(btload_int(length, btend));
    } else {
        int count = (length_end - length - 2);
        for (i=0; i<count-9; i++) {
            value *= 10;
            value += (length[i+1]-'0');
        }
        for (;i<count;i++) {
            offset *= 10;
            offset += (length[i+1]-'0');
        }
        add_piece_length(offset);
        /* may be should use shift */
        for (i=0; i<value; i++) {
            add_piece_length(1000000000);
        }
    }
    return 0;
}

static int load_dht_node(const char *node, const char *node_end)
{
    const char *address = btload_list(node, node_end, 0);
    const char *port = btload_list(node, node_end, 1);
    const char *daddress = btstr_dup(address, node_end);
    const int iport = btload_int(port, node_end);
    if (daddress != NULL && iport != -1){
       dht_add_node(daddress, iport);
    }
    delete[] daddress;
    return 0;
}

static int load_nodes(const char *nodes, const char *nodes_end)
{ 
   int i = 0;
   const char *node = btload_list(nodes, nodes_end, i++);
   while (node < nodes_end) {
       node = btload_list(nodes, nodes_end, i++);
       load_dht_node(node, nodes_end);
    }
    return 0;
}

static unsigned char *__static_block_hash;
int load_torrent(const char *path)
{
    static char __static_torrent_base[2*1024*1024];
    int count = bfload(path, __static_torrent_base,
                       sizeof(__static_torrent_base));
    const char *torrent_limited = __static_torrent_base+count;
    const char *announce_list = btload_dict(__static_torrent_base,
                                            torrent_limited, "13:announce-list");

    tfdump_info_file_list(__static_torrent_base, count);
    if (announce_list < torrent_limited) {
        int i = 0;
        const char *announce = btload_list(announce_list, torrent_limited, i++);
        while (announce < torrent_limited) {
            do_add_announce(announce, torrent_limited);
            announce = btload_list(announce_list, torrent_limited, i++);
        }
    } else {
        const char *announce = btload_dict(__static_torrent_base,
                                           torrent_limited, "8:announce");
        if (announce < torrent_limited) {
            do_add_announce0(announce, torrent_limited);
        }
    }
    const char *info = btload_dict(__static_torrent_base,
                                   torrent_limited, "4:info");
    if (info > torrent_limited) {
        return -1;
    }
    const char *info_end = btcheck_dict(info, torrent_limited);
    if (info_end < torrent_limited) {
        unsigned char digest[20];
        SHA1Hash(digest, (unsigned char*)info, info_end-info);
        update_session(digest);
        printf("info_hash: %s\n", get_info_hash());
    }
    const char *piece_length = btload_dict(info,
                                           torrent_limited, "12:piece length");
    if (piece_length < torrent_limited) {
        __piece_size = btload_int(piece_length, torrent_limited);
        printf("piece length: %d\n", __piece_size);
    }
    __piece_count = 0;
    __last_piece_size = 0;
    const char *name = btload_dict(info, torrent_limited, "4:name");
    const char *name_utf8 = btload_dict(info, torrent_limited, "10:name.utf-8");
#ifdef UTF_8
    if (name_utf8 < torrent_limited) {
        name = name_utf8;
    }
#endif
    const char *base = btstr_dup(name, torrent_limited);
    char *buf = new char[4+strlen(base)];
    sprintf(buf, "%s.bt", base);
    __config_name = buf;
    const char *du = btload_dict(info, torrent_limited, "6:length");
    struct _disk_file_t dist_file;
    memset(&dist_file, 0, sizeof(dist_file));
    if (du < torrent_limited) {
        dist_file.df_first_byte = __last_piece_size;
        dist_file.df_first_piece = __piece_count;
        __last_piece_size = btload_int(du, torrent_limited);
        __piece_count += (__last_piece_size/__piece_size);
        __last_piece_size %= __piece_size;
        dist_file.df_last_byte = __last_piece_size;
        dist_file.df_last_piece = __piece_count;
        dist_file.df_local_path = strdup(base);
        save_local_file_path(&dist_file);
    } else {
        int length0 = 0;
        const char *files = btload_dict(info, torrent_limited, "5:files");
        if (files < torrent_limited) {
            int i = 0;
            const char *length = btload_list(files, torrent_limited, i++);
            while (length < torrent_limited) {
                dist_file.df_first_byte = __last_piece_size;
                dist_file.df_first_piece = __piece_count;
                load_file_length(length, torrent_limited, base, &dist_file);
                dist_file.df_last_byte = __last_piece_size;
                dist_file.df_last_piece = __piece_count;
                save_local_file_path(&dist_file);
                length = btload_list(files, torrent_limited, i++);
            }
        }
    }
    delete[] base;
    printf("total byte: %dx%d+%d\n",
           __piece_size, __piece_count, __last_piece_size);
    const char *hash_table = btload_dict(info, torrent_limited, "6:pieces");
    if (hash_table < torrent_limited) {
        int count;
        const char *hash = btload_str(hash_table, torrent_limited, &count);
        __static_block_hash = new unsigned char[count];
        memcpy(__static_block_hash, hash, count);
        if (__last_piece_size > 0) {
            count -= 20;
        }
        if (count != __piece_count*20) {
            assert(0&&"bad hash table");
        }
    }
    const char *nodes = btload_dict(__static_torrent_base,
	 torrent_limited, "5:nodes");
    if (nodes < torrent_limited){
	load_nodes(nodes, torrent_limited);
    }
    return 0;
}

int LoadTorrentPeer(char *torrent, int len)
{
    int interval0 = 300;
    extern int loadPeers(char *buf, int len);
    const char *btend = torrent+len;
    const char *btok = btcheck(torrent, btend);
    if (btok > btend) {
        return -1;
    }
    const char *reason = btload_dict(torrent, btend, "14:failure reason");
    if (reason < btend) {
        const char *dp = btstr_dup(reason, btend);
        printf("fail: %s\n", dp);
        delete[] dp;
        return -1;
    }
    const char *peers = btload_dict(torrent, btend, "5:peers");
    if (peers < btend) {
        int count;
        const char *peer_list = btload_str(peers, btend, &count);
        if (peer_list < btend) {
            loadPeers((char*)peer_list, count);
        }
    }
    const char *interval = btload_dict(torrent, btend, "8:interval");
    if (interval < btend) {
        interval0 = btload_int(interval, btend);
        printf("interval: %d\n", interval0);
    }

    return interval0;
}

void check_hash(int index, const char *buff, int count)
{
    static int last_ok = 0;
    static int check_count = 0;
    static int check_ok = 0;
    if (count > 0) {
        unsigned char degist[20];
        SHA1Hash(degist, (unsigned char*)buff, count);
        if (memcmp(degist, __static_block_hash+20*index, 20) == 0) {
            extern int bitfield_set(char bitField[], int index);
            bitfield_set(__bitField, index);
            check_ok++;
        }
        check_count++;
        if (last_ok<check_ok && (check_count%10)==0) {
            last_ok = check_ok;
            fprintf(stderr, "\rcheck ok: %d/%d", check_ok, check_count);
        }
    }
}

int file_checksum(_disk_file_t *dist_file, char *buff, int byte_in_buff, int force_uncheck)
{
    printf("check file: %s\n", dist_file->df_local_path);
    FILE *fp = fopen(dist_file->df_local_path, "rb+");
    if (fp == NULL) {
        printf("fail file: %s\n", dist_file->df_local_path);
        return 0;
    }
    dist_file->df_cache_file = fp;
    if (force_uncheck == 1){
        return 0;
    }
    int offset = dist_file->df_first_byte;
    int index = dist_file->df_first_piece;
    while (feof(fp) == 0) {
        int count = fread(buff, 1, __piece_size-offset, fp);
        if (count > 0) {
            offset += count;
        }
        if (offset >= __piece_size) {
            check_hash(index++, buff, offset);
            offset = 0;
        }
    }
    return offset;
}

int do_random_access_read_bench()
{
    int i;
    int count;
    static char buffer[1024*1024*8];
    for (i=0; i<1000; i++) {
        int index = rand()%__piece_count;
        extern char __bitField[];
        extern int bitfield_test(const char bitField[], int index);
        if (bitfield_test(__bitField, index)) {
            int count = segbuf_read(index, 0, buffer,  __piece_size);
            unsigned char digest[20];
            assert(count == __piece_size);
            SHA1Hash(digest, (unsigned char*)buffer, __piece_size);
            if (memcmp(digest, __static_block_hash+20*index, 20) == 0) {
                printf("random access success!\n");
            } else {
                printf("random access fail!\n");
            }
        }
    }
    return 0;
}

void image_checksum()
{
    int i;
#if 1
    int count = 0;
    int force_uncheck = 0;
    FILE *fp = fopen(__config_name, "rb");
    if (fp != NULL){
        int blen = (__last_piece_size?__piece_count+8:__piece_count+7);
        fread(__bitField, blen/8, 1, fp);
        fclose(fp);
        force_uncheck = 1;
    }
    char *buff = new char[__piece_size];
    for (i=0; i<__local_disk_file_count; i++) {
        count = file_checksum(&__local_disk_files[i], buff, count, force_uncheck);
    }
    if (force_uncheck == 0){
           check_hash(__local_disk_files[__local_disk_file_count-1].df_last_piece,
                   buff, count);
    }
    delete[] buff;
#endif
    extern int loadPeers(char *, int);
    loadPeers(NULL, 0);
}

static int large_file_seek(FILE *file, int piece, int size, int start)
{
    int i;
    int recal_size = size;
    int recal_piece = piece;
    int recal_start = start;
    while (recal_piece>2 && recal_size<1024*1024*1024) {
        if (recal_piece & 0x1) {
            recal_start += recal_size;
        }
        recal_piece >>= 1;
        recal_size <<= 1;
    }
    fseek(file, 0, SEEK_SET);
    for (i=0; i<recal_piece; i++) {
        fseek(file, recal_size, SEEK_CUR);
    }
    fseek(file, recal_start, SEEK_CUR);
    return 0;
}

int large_file_write(int piece, int start, const char *buf, int length)
{
    int i = 0;
    for (i=0; i<__local_disk_file_count; i++) {
        int large = piece - __local_disk_files[i].df_first_piece;
        if (large == 0) {
            large = start - __local_disk_files[i].df_first_byte;
        }
        int large2 = piece - __local_disk_files[i].df_last_piece;
        if (large2 == 0) {
            large2 = start - __local_disk_files[i].df_last_byte;
        }
        if (large>=0 && large2<0) {
            if (__local_disk_files[i].df_cache_file == NULL) {
                __local_disk_files[i].df_cache_file = cacheopen(
                                                          __local_disk_files[i].df_local_path,
                                                          "wb+");
            }
            __static_file = __local_disk_files[i].df_cache_file;
            large_file_seek(__static_file,
                            piece-__local_disk_files[i].df_first_piece,
                            __piece_size,
                            start-__local_disk_files[i].df_first_byte);
            int count = __local_disk_files[i].df_last_piece>piece?
                        length:(__local_disk_files[i].df_last_byte>start+length?length:
                                __local_disk_files[i].df_last_byte-start);
            fwrite(buf, 1, count, __static_file);
            if (count < length) {
                large_file_write(piece, start+count, buf+count, length-count);
            }
            break;
        }
    }
    return 0;
}

static int __uploaded = 0;
int get_uploaded()
{
    return __uploaded;
}

int segbuf_read(int piece, int start, char buf[], int length)
{
    int i = 0;
    int count = 0;
    for (i=0; i<__local_disk_file_count; i++) {
        int large = piece - __local_disk_files[i].df_first_piece;
        if (large == 0) {
            large = start - __local_disk_files[i].df_first_byte;
        }
        int large2 = piece - __local_disk_files[i].df_last_piece;
        if (large2 == 0) {
            large2 = start - __local_disk_files[i].df_last_byte;
        }
        if (large>=0 && large2<0) {
            if (__local_disk_files[i].df_cache_file == NULL) {
                return -1;
            }
            __static_file = __local_disk_files[i].df_cache_file;
            large_file_seek(__static_file,
                            piece-__local_disk_files[i].df_first_piece,
                            __piece_size,
                            start-__local_disk_files[i].df_first_byte);
            count = __local_disk_files[i].df_last_piece>piece?
                    length:(__local_disk_files[i].df_last_byte>start+length?length:
                            __local_disk_files[i].df_last_byte-start);
            fread(buf, 1, count, __static_file);
            if (count < length) {
                int error = segbuf_read(piece,
                                        start+count, buf+count, length-count);
                if (error == -1) {
                    return -1;
                }
                count += error;
            }
            break;
        }
    }
    __uploaded += count;
    return count;
}

void save_config()
{
    FILE *fp = fopen(__config_name, "wb");
    if (fp != NULL){
        int blen = (__last_piece_size?__piece_count+8:__piece_count+7);
        fwrite(__bitField, blen/8, 1, fp);
        fclose(fp);
        printf("config save ok\n");
        return ;
    }
    return ;
}

#endif
