/* 
 * Copyright 2003,2004,2005 Kevin Smathers, All Rights Reserved
 *
 * This file may be used or modified without the need for a license.
 *
 * Redistribution of this file in either its original form, or in an
 * updated form may be done under the terms of the GNU GENERAL
 * PUBLIC LICENSE.  If this license is unacceptable to you then you
 * may not redistribute this work.
 * 
 * See the file COPYING for details.
 */

#include "config.h"

#include "sha1.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "bts.h"
#include "types.h"
#include "benc.h"

#define V(vers) #vers

int main( int argc, char **argv) {
    char *fname;
    btStream *in;
    btObject *o;
    btString *s;
    btInteger *i;
    btList *l;
    btDict *d;
    int idx;
    int optdebug = 0;
    int opthelp = 0;
    int opt;

    while ((opt = getopt( argc, argv, "d")) != -1) {
	switch (opt) {
	    case 'd':
	    	optdebug = 1;
		break;
	    default:
		printf("Unknown option '%c'\n", opt);
		opthelp = 1;
		break;
	}
    }

    if (optind >= argc || opthelp) {
	printf("Usage: btlist [options] <torrent>...\n");
	printf("Version: %.2f\n", VERSION);
	printf("Options:\n");
	printf("  -d            Debug dump\n");
	exit(1);
    }

    fname = argv[optind];
    in = bts_create_filestream( fname, BTS_INPUT);

    if (benc_get_object( in, &o)) {
       printf("read failed.\n");
       exit(1);
    } 

    /* 
    * Metainfo files are bencoded dictionaries with the following keys -
    * 
    *    md={
    *      announce=>'url',
    *      info=>{
    *          name=>'top-level-file-or-directory-name',
    *          piece length=>12345,
    *          pieces=>'md5sums',
    * 
    *          length=>12345,      
    *            -or-
    *          files=>[
    *              {
    *                  length=>12345,
    *                  path=>['sub','directory','path','and','filename']
    *                           
    *              }, ... {}
    *          ]
    *      }
    * 
    */

    printf("metainfo file.: %s\n", fname);

    if (optdebug) {
	btObject_dump(0,o);
    }

    {
       /* SHA1 */ 
       btStream *tmpbts; 
       unsigned char digest[SHA_DIGEST_LENGTH];
       struct btstrbuf out;

       d=BTDICT( btObject_val( o, "info"));
       tmpbts = bts_create_strstream();
       benc_put_dict( tmpbts, d);
       out = bts_get_buf( tmpbts);
       SHA1( out.buf, out.len, digest);
       printf("info hash.....: ");
       for (idx=0; idx<sizeof(digest); idx++) {
	   printf("%02x",digest[idx]);
       }
       bts_destroy( tmpbts);
       printf("\n");
    }

    i=BTINTEGER( btObject_val(o, "info/length"));
    if (i) {
       /* file mode */
       btInteger *pl;
       s=BTSTRING( btObject_val( o, "info/name.utf-8")); 
       s||(s=BTSTRING( btObject_val( o, "info/name"))); 
       printf("file name.....: %s\n", s->buf);
       s=BTSTRING( btObject_val( o, "info/pieces"));
       pl=BTINTEGER( btObject_val(o, "info/piece length"));
       printf("file size.....: %lld (%lld * %lld + %lld)\n", i->ival, i->ival/pl->ival, pl->ival, i->ival % pl->ival);
    } else {
       /* dir mode */
       _int64 tsize=0;
       s=BTSTRING( btObject_val( o, "info/name.utf-8")); 
       s||(s=BTSTRING( btObject_val( o, "info/name"))); 
       printf("directory name: %s\n", s->buf);

       l=BTLIST( btObject_val(o, "info/files"));
       printf("files.........: %d\n", l->len);

       for (idx=0; idx<l->len; idx++) {
	   int pathel;
	   btList *filepath;
	   btInteger *filesize;
	   filepath=BTLIST( btObject_val( l->list[idx], "path.utf-8"));
	   filepath||(filepath=BTLIST( btObject_val( l->list[idx], "path")));
	   filesize=BTINTEGER( btObject_val( l->list[idx], "length"));
	   printf("   ");
	   for (pathel=0; pathel<filepath->len; pathel++) {
	       if (pathel>0) printf("/");
	       printf("%s", BTSTRING(filepath->list[pathel])->buf);
	   }
	   printf(" (%lld)\n", filesize->ival);
	   tsize+=filesize->ival;
       }

       s=BTSTRING( btObject_val( o, "info/pieces"));
       i=BTINTEGER( btObject_val(o, "info/piece length"));
       printf("archive size..: %lld (%lld * %lld + %lld)\n",
	   tsize, tsize/i->ival, i->ival, tsize%i->ival);
    }

    s=BTSTRING( btObject_val( o, "announce"));
    printf("announce url..: %s\n", s->buf);

    printf("\n");

    btObject_destroy( o);
    bts_destroy( in);
    return 0;
}
