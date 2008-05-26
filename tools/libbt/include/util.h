#ifndef __UTIL__H
#define __UTIL__H
#include <errno.h>
#include <sys/types.h>
#define DIE(msg) die(__FILE__, __LINE__, msg, 0)
#define SYSDIE(msg) die(__FILE__, __LINE__, msg, errno)
#define DIE_UNLESS(expr) (void)((expr) || die(__FILE__, __LINE__, #expr, 0))

#if !WIN32
typedef long long _int64;
#endif

void *btmalloc( size_t size);
void *btcalloc( size_t nmemb, size_t size);
void *btrealloc( void *buf, size_t size);
void btfree( void *buf);


char *bts_strerror( int err);
void bts_perror( int err, char *msg);
int die( char *file, int line, char *msg, int err);
int openPath( char *path, int flags);
int cacheopen( char *path, int flags, int mode);
void cacheclose( void);
void hexencode (const unsigned char *digest, int len, char *buf, int buflen);
int hexdecode (unsigned char *digest, int len, const char *buf, int buflen);
#if WIN32 || !HAVE_ON_EXIT
	typedef void (*exitfn_ptr) (int,void*);
	int on_exit( exitfn_ptr exitfn, void* data) ;
#endif
#if 0
#define malloc(s) use_btmalloc_instead
#define calloc(n,s) use_btcalloc_instead
#define realloc(p,s) use_btrealloc_instead
#define free(p) use_btfree_instead
#endif

#endif
