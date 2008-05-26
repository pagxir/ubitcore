/* 
 * Copyright 2003,2004,2005 Kevin Smathers, All Rights Reserved
 *
 * This file may be used or modified without the need for a license.
 *
 * Redistribution of this file in either its original form, or in an
 * updated form may be done under the terms of the GNU LIBRARY GENERAL
 * PUBLIC LICENSE.  If this license is unacceptable to you then you
 * may not redistribute this work.
 * 
 * See the file COPYING.LGPL for details.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !WIN32
#   if HAVE_UNISTD_H
#      include <unistd.h>
#   endif
#   if HAVE_FCNTL_H
#      include <fcntl.h>
#   endif
#   include <sys/param.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#if WIN32
#   include <io.h>
#   include <direct.h>
#   define MAXPATHLEN 255
#   define MKDIR(p,f) mkdir(p)
#else
#   define MKDIR(p,f) mkdir(p,f)
#endif
#include "bterror.h"
#include "util.h"
#undef malloc
#undef calloc
#undef realloc
#undef free


#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif /* WITH_DMALLOC */
/* Globals */
#define FILESETSIZE 50
static struct {
    int fd;
    int options;
    char *path;
} c_fileset[ FILESETSIZE ];
static int c_index = 0;

void *btcalloc( size_t nmemb, size_t size) {
    void *region = calloc( nmemb, size);
    if (!region) SYSDIE("calloc");
    return region;
}

void *btmalloc( size_t size) {
    void *region = malloc( size);
    if (!region) SYSDIE("malloc");
    return region;
}

void *btrealloc( void *ptr, size_t size) {

    void *region = realloc( ptr, size);
    if (!region) SYSDIE("realloc");
    return region;
}

void btfree( void *ptr) {
    free( ptr);
}

char *bts_strerror( int err) {
    if (err) {
	if (err >= BTERR_BASE && err < BTERR_LAST) {
	    return bterror_string[err - BTERR_BASE];
	} else {
	    return strerror(err);
	}
    } 
    return "Not an error";
}

void bts_perror( int err, char *msg) {
    fprintf( stderr, "%s: [%d] %s\n", msg, err, bts_strerror(err));
}

int die( char *file, int line, char *msg, int err) {
    if (err) {
	fprintf( stderr, "%s+%d: (FATAL ERROR) %s ([%d] %s)\n", 
	    file, line, msg, err, bts_strerror( err));
    } else {
	fprintf( stderr, "%s+%d: (FATAL ERROR) %s\n", 
	    file, line, msg);
    }
    abort();
}

int
cacheopen( char *path, int options, int mode) {
    int i;
    int fd;
    int err;
    static int last_i = 0;

    if (c_fileset[last_i].path == path && c_fileset[last_i].options == options) 
    {
	return c_fileset[last_i].fd;
    }
    for (i=0; i<FILESETSIZE; i++) {
        if (path == c_fileset[i].path && options == c_fileset[i].options) {
	    last_i = i;
	    return c_fileset[i].fd;
	}
    }
    printf("cacheopen(%s)\n", path);
    fd = open( path, options, mode);
    if (fd < 0) return fd;
    if (c_fileset[c_index].fd > 0) {
        err = close(c_fileset[c_index].fd);
	if (err) {
	    bts_perror( errno, "Error closing file");
	    fprintf( stderr, "File '%s' may be corrupt\n", c_fileset[c_index].path);
	}
    }
    c_fileset[c_index].fd = fd;
    c_fileset[c_index].path = path;
    c_fileset[c_index].options = options;
    c_index = (c_index + 1) % FILESETSIZE; 
    return fd;
}

void cacheclose( void) {
    int err;
    int i;
    for (i=0; i<FILESETSIZE; i++) {
        if (c_fileset[c_index].fd) {
	    err = close(c_fileset[c_index].fd);
	    c_fileset[c_index].path = NULL;
	    c_fileset[c_index].options = 0;
	    c_fileset[c_index].fd = 0;
	    if (err) {
		bts_perror( errno, "Error closing file");
		fprintf( stderr, "File '%s' may be corrupt\n", c_fileset[c_index].path);
	    }
	}
    }
}

int openPath( char *path, int options) {
    char subdir[MAXPATHLEN];
    char *sep;
    int fd;
    int len;
    int err;
#if 0
    printf("open( %s,...)\n", path);
#endif
    fd = cacheopen( path, options, 0666);
    if (fd < 0) {
#if 0
	perror("open failed");
#endif
	sep = path;

	sep = strchr(sep+1, '/');

	while( sep) {
	    len = sep - path;
	    memcpy( subdir, path, len);
	    subdir[len] = 0;
#if 0
	    printf("mkdir( %s,...)\n", subdir);
#endif
	    err = MKDIR(subdir, 0777);
	    if (err) {
	        if (errno == EEXIST) {
		    /* check for existing directory */
		    int mderr = errno;
		    int serr;
		    struct stat sbuf;
		    serr = stat(subdir, &sbuf);
		    if (!serr && S_ISDIR(sbuf.st_mode)) err = 0;
		    errno=mderr;
		}
		if (err) {
		    bts_perror( errno, "Error creating directory");
		    fprintf( stderr, "Directory '%s' will be missing\n", path);
		}
	    }
	    sep = strchr(sep+1, '/');
	}
#if 0
	printf("open( %s,...)\n", path);
#endif
	fd = cacheopen( path, options, 0666);
    }
    if (fd < 0) {
	bts_perror( errno, "Error creating file");
	fprintf( stderr, "File '%s' will be missing\n", path);
    }
    return fd;
}

void
hexencode (const unsigned char *digest, int len, char *buf, int buflen) {
    int i;
    for (i=0; i<len; i++) {
	snprintf(buf+3*i, buflen - 3*i, "%%%02x", digest[i]);
    }
}

int
hexdecode (unsigned char *digest, int len, const char *buf, int buflen) {
    int i;
    const char *cpos = buf;
    for (i=0; i<len; i++) {
        unsigned int hexval;
	int nextchar;
	if (sscanf( cpos, "%%%02x%n", &hexval, &nextchar) < 1) {
	    return -1;
	}
	digest[i] = (unsigned char)hexval;
        cpos += nextchar;
    }
    if (*cpos != 0 && *cpos != '\n' && *cpos != '\r') return -2;
    return 0;
}

#if WIN32 || !HAVE_ON_EXIT
#define EXITMAX 20
static struct {
    exitfn_ptr exitfn;
    void *data;
} __exitlist[EXITMAX];
static int __exitindex = 0;

void on_exit_exit( void) {
    int i;
    for (i=0; i<__exitindex; i++) {
	__exitlist[i].exitfn( 0, __exitlist[i].data);
    }
}

int on_exit( exitfn_ptr exitfn, void* data) {
    if (__exitindex >= EXITMAX) return 1;
    if (__exitindex == 0) {
	atexit( on_exit_exit);
    }
    __exitlist[ __exitindex].exitfn = exitfn;
    __exitlist[ __exitindex].data = data;
    __exitindex++;
    return 0;
}
#endif
