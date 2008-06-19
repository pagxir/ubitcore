#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "btcodec.h"


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

btcodec::btcodec()
{
    b_len = 0;
    b_text = NULL;
}

btcodec::~btcodec()
{
    delete[] b_text;
}

int btcodec::bload(const char *buffer, int len)
{
    if (b_len < len){
        delete[] b_text;
        b_text = new char[len];
    }
    b_len = len;
    assert(b_text != NULL);
    memcpy(b_text, buffer, len);
    return 0;
}

bentity& btcodec::bget()
{
    return __INFINITE;
}

bentity& bentity::bget(int index)
{
    return __INFINITE;
}

bentity& bentity::bget(const char *name)
{
    return __INFINITE;
}



const char *bentity::b_str(size_t *len)
{
    return NULL;
}

const char *bentity::c_str(size_t *len)
{
    return NULL;
}

bentity __INFINITE;
