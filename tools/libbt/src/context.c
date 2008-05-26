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

/* context.c */
#include "config.h"

#include <curl/curl.h>
#include <curl/easy.h>
#include "sha1.h"
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#if WIN32
#   include <winsock2.h>
#else
#   include <sys/param.h>
#   include <sys/socket.h>
#   include <unistd.h>
#   include <netdb.h>
#endif
#include <time.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "context.h"
#include "bts.h"
#include "types.h"
#include "benc.h"
#include "random.h"
#include "peer.h"
#include "stream.h"
#include "util.h"
#include "segmenter.h"

#define MIN_PORT 6881
#define MAX_PORT 6889
#if WIN32
#   define snprintf _snprintf
#endif

static int parse_config_digest( 
	const char *file, int lineno, const char *token, 
	unsigned char *digest, int len, 
	const char *linebuf) 
{
    int tlen = strlen(token);
    const char *line;
    int res=-1;
    if (strncmp( linebuf, token, tlen) == 0) {
	line = linebuf + tlen;
	line += strspn( line, " \t");
	res = hexdecode( digest, len, line, strlen(line));
	if (res) {
	    printf("In configuration file %s, line %d, the token %s failed to decode (%d)\n", file, lineno, token, res);
	    abort();
	}
    }
    return res;
}

btContext *btContext_create( btContext *ctx, float ulfactor, char *rcfile) {
    FILE *config;
    char hexbuf[80];
    int lineno = 0;
#ifdef BROKEN_HOSTNAME_CODE
    char host[80];
    struct hostent* hent;
    char myaddr[16];
#endif
    int i;

    if (!ctx) {
        ctx = btmalloc( sizeof(*ctx));
    }
    /* default initialize the entire context */
    memset( ctx, 0, sizeof(*ctx));
    /* initialize the socket to status map */
    for (i=0; i<SOCKID_MAX; i++) {
	ctx->statmap[i] = -1;
    }

    /* read defaults */
    config=fopen(rcfile, "r");
    if (!config) {
        randomid( ctx->myid, IDSIZE);
        randomid( ctx->mykey, KEYSIZE);
        config=fopen(".libbtrc", "w");

        hexencode( ctx->myid, IDSIZE, hexbuf, sizeof(hexbuf));
        fprintf( config, "myid: %s\n", hexbuf);
        hexencode( ctx->mykey, KEYSIZE, hexbuf, sizeof(hexbuf));
        fprintf( config, "mykey: %s\n", hexbuf);
        fclose( config);
    } else {
	int got_id=0, got_key=0;

        while (!feof(config)) {
            lineno++;
            fgets( hexbuf, sizeof(hexbuf), config);
            if (parse_config_digest( rcfile, lineno, "myid:", ctx->myid, IDSIZE, hexbuf)==0) {
                got_id=1;
            }
            if (parse_config_digest( rcfile, lineno, "mykey:", ctx->mykey, KEYSIZE, hexbuf)==0) {
                got_key=1;
            }
        } /* while */

	if (!got_id || !got_key) {
	    fprintf(stderr, "Please remove rcfile '%s'\n", rcfile);
	    DIE("Bad configuration file.");
        }

	fclose(config);
    }

    ctx->ulfactor = ulfactor;
    ctx->listenport = MIN_PORT;

#ifdef BROKEN_HOSTNAME_CODE
    /* look up my ip address */
    if (gethostname( host, sizeof(host)) != 0) {
	bts_perror(errno, "gethostname");
	exit(1);
    }
    hent = gethostbyname( host);
    if (!hent) {
	printf("Fatal error, unable to look up hostname '%s'\n", host);
	herror("Host look up failed");
	exit(1);
    }
    sprintf(myaddr, "%d.%d.%d.%d", 
	    hent->h_addr[0],
	    hent->h_addr[1],
	    hent->h_addr[2],
	    hent->h_addr[3]);
    ctx->ip = strdup(myaddr); 
#endif

    return ctx;
}

void ctx_closedownload(btContext *ctx, unsigned download) {
  int i;
  btDownload *dl=ctx->downloads[download];
  btPeerset *pset = &dl->peerset;

  for (i=0; i<pset->len; i++) {
      if (pset->peer[i] != NULL) {
	  peer_shutdown (ctx, pset->peer[i], "exiting");
	  btfree(pset->peer[i]);
	  pset->peer[i] = NULL;
      }
  }
  btfree(pset->peer);
  pset->peer = NULL;

  btfree (dl->url);
  btFileSet_destroy( &dl->fileset);
  kBitSet_finit( &dl->requested);
  kBitSet_finit( &dl->interested);
  if(dl->md)
    btObject_destroy( dl->md);
  ctx->downloads[download]=NULL;
  btfree(dl);
}

void btContext_destroy( btContext *ctx) {
    int i;

    for(i=0; i<ctx->downloadcount; i++) {
      if(ctx->downloads[i])
	ctx_closedownload(ctx, i);
    }

    /* Note: btfree(NULL) is a no-op */
    btfree (ctx->downloads);
    /* Shouldn't be necessary - one must recreate the context */
    ctx->downloads=NULL;
    ctx->downloadcount=0;
}

static char* hexdigest(unsigned char *digest, int len) {
    int i;
    char *buf = btmalloc(3*len+1);
    for (i=0; i<len; i++) {
	sprintf(buf+3*i, "%%%02x", digest[i]);
    }
    return buf;
}

int
btresponse( btContext *ctx, int download, btObject *resp) {
    int ret = 0;
    btDownload *dl=ctx->downloads[download];
    btString *err;
    btPeer *p;
    int i,j, skip=0;
    struct sockaddr_in target;


    err = BTSTRING(btObject_val( resp, "failure reason"));
    if (err) {
	printf("Error from tracker: %s\n", err->buf);
	ret = -1;
    } else {
        btObject *peersgen;
	btInteger *interval;
	interval = BTINTEGER( btObject_val( resp, "interval"));
	dl->reregister_interval = (int)interval->ival;
	printf("Interval %lld\n", interval->ival);
	peersgen = btObject_val( resp, "peers");
	if (peersgen->t == BT_LIST) {
	    btList *peers = BTLIST( peersgen);
	    for (i=0; i<peers->len; i++) {
		btObject *o = peers->list[i];
		btString *peerid = BTSTRING( btObject_val( o, "peer id"));
		btString *ip = BTSTRING( btObject_val( o, "ip"));
		btInteger *port = BTINTEGER( btObject_val( o, "port"));
		int iport = (int)port->ival;
		const struct addrinfo ai_template={
		  .ai_family=AF_INET,	/* PF_INET */
		  .ai_socktype=SOCK_STREAM,
		  /*.ai_protocol=IPPROTO_TCP,*/
		};
		struct addrinfo *ai=NULL;

		skip=0;

		if (memcmp(ctx->myid, peerid->buf, IDSIZE)==0) {
		    printf("Skipping myself %s:%d\n", ip->buf, iport);
		    continue;
		}
		
		/* TODO: detect collisions properly (incl. ourselves!) */
		for (j = 0; j < dl->peerset.len; j++) {
		    btPeer *p = dl->peerset.peer[j];
		    if ( memcmp( p->id, peerid->buf, IDSIZE)==0) {
			/* Simply tests by ID. */
			fprintf( stderr, 
				   "Skipping old peer: %s:%d", inet_ntoa(target.sin_addr), ntohs(target.sin_port));
			skip = 1;
			break;
		    }
		}
		if (skip) continue;

		printf( "Contacting peer %s:%d\n", ip->buf, iport);
		if(getaddrinfo(ip->buf, NULL, &ai_template, &ai)==0) {	
		    /* Just pick the first one, they are returned in varying order */
		    p = peer_add( ctx, download, peerid->buf, &((struct sockaddr_in *)ai->ai_addr)->sin_addr, iport);
		}
		if(ai) {
		    freeaddrinfo(ai);
		}
	    }
	} else if (peersgen->t == BT_STRING) {
	    btString *peers=BTSTRING(peersgen);

	    fprintf( stderr, "Parsing compact peer list\n");
	    for (i=0; i<=peers->len - 6; i += 6) {
		struct sockaddr_in target;

		skip = 0;

		/* Collect the address */
		target.sin_family=AF_INET;
		memcpy(&target.sin_addr, peers->buf+i, 4);
		memcpy(&target.sin_port, peers->buf+i+4, 2);

		/* TODO: detect collisions properly (incl. ourselves!) */
		for (j = 0; j < dl->peerset.len; j++) {
		    btPeer *p = dl->peerset.peer[j];
		    const struct sockaddr_in *a=(const struct sockaddr_in*)&p->ip;
		    if (a->sin_family==target.sin_family) {
			/* Simply tests by IP. */
			if(memcmp(&a->sin_addr, &target.sin_addr, sizeof(a->sin_addr))==0) {
			    fprintf( stderr, 
				   "Skipping old peer: %s:%d", inet_ntoa(target.sin_addr), ntohs(target.sin_port));
			    skip = 1;
			    break;
			}
		    }
		}
		if (skip) continue;

		p = peer_add( ctx, download, "", &target.sin_addr, ntohs(target.sin_port));
	    } /* for */
	
	}
/*	peer_dump( &ctx->peerset); */
    }
    return ret;
}

static btObject* btrequest( 
	char *announce,
	unsigned char myid[IDSIZE], 
	unsigned char mykey[KEYSIZE],
	unsigned char digest[SHA_DIGEST_LENGTH], 
	int port,
	_int64 downloaded, 
	_int64 uploaded,
	_int64 left,
	char *event) 
{
    /*
    * Tracker GET requests have the following keys urlencoded -
    *   req = {
    *REQUEST
    *      info_hash => 'hash'
    *      peer_id => 'random-20-character-name'
    *      port => '12345'
    *      ip => 'ip-address' -or- 'dns-name'  iff ip
    *      uploaded => '12345'
    *      downloaded => '12345'
    *      left => '12345'
    *
    *      last => last iff last
    *      trackerid => trackerid iff trackerid
    *      numwant => 0 iff howmany() >= maxpeers
    *      event => 'started', 'completed' -or- 'stopped' iff event != 'heartbeat'
    *
    *   }
    */

    /* contact tracker */
    CURL *hdl;
    char url[1024];
    char *dgurl;
    char *idurl;
    char *keyurl;
    btStream *io;
    btObject *result;
    int curlret;
    dgurl=hexdigest(digest, SHA_DIGEST_LENGTH);
    idurl=hexdigest(myid, IDSIZE);
    keyurl=hexdigest(mykey, KEYSIZE);
    hdl = curl_easy_init();
    if (event) {
	snprintf( url, sizeof(url)-1, "%s?info_hash=%s&peer_id=%s&key=%s&port=%d&uploaded=%lld&downloaded=%lld&left=%lld&event=%s&compact=1", 
		announce,
		dgurl,
		idurl,
		keyurl,
		port,
		uploaded, 
		downloaded,
		left,
		event
	    );
    } else {
	snprintf( url, sizeof(url)-1, "%s?info_hash=%s&peer_id=%s&key=%s&port=%d&uploaded=%lld&downloaded=%lld&left=%lld&compact=1", 
		announce,
		dgurl,
		idurl,
		keyurl,
		port,
		uploaded, 
		downloaded,
		left
	    );
    }
    url[sizeof(url)-1]=0;
    btfree(idurl); btfree(dgurl); btfree(keyurl);
    printf("get %s\n", url);
    curl_easy_setopt( hdl, CURLOPT_URL, url);
    io = bts_create_strstream( BTS_OUTPUT);
    curl_easy_setopt( hdl, CURLOPT_FILE, io);
    curl_easy_setopt( hdl, CURLOPT_WRITEFUNCTION, writebts);
    if ((curlret = curl_easy_perform( hdl)) != CURLE_OK)
    {
      switch (curlret)
      {
      case CURLE_COULDNT_CONNECT:
	fprintf(stderr, "Failed to transfer URL: could not connect (%d)\n", curlret);
      default:
	fprintf(stderr, "Failed to transfer URL for reason %d (see curl.h)\n", curlret);
      }
      result=NULL;
    }
    else
    {
      /* parse the response */
      if (bts_rewind( io, BTS_INPUT)) DIE("bts_rewind");
      if (benc_get_object( io, &result)) DIE("bad response");
    }
    bts_destroy (io);
    curl_easy_cleanup( hdl);
    return result;
}

int ctx_addstatus( btContext *ctx, int fd) {
    int statblock;

    DIE_UNLESS(fd>=0 && fd<=SOCKID_MAX); /* include TMPLOC */

    /* allocate status bits */
    statblock = ctx->nstatus;
    if (statblock >= MAXCONN) {
	return -1;
    }
    ctx->nstatus++;

    ctx->statmap[ fd] = statblock;
    ctx->status[ statblock].fd = fd;
    ctx->status[ statblock].events = 0;;
    return 0;
}

void 
ctx_setevents( btContext *ctx, int fd, int events) {
    int statblock;

    DIE_UNLESS(fd>=0 && fd<SOCKID_MAX);

    statblock = ctx->statmap[ fd];
    ctx->status[ statblock].events |= events;
}

void 
ctx_clrevents( btContext *ctx, int fd, int events) {
    int statblock;

    DIE_UNLESS(fd>=0 && fd<SOCKID_MAX);

    statblock = ctx->statmap[ fd];
    ctx->status[ statblock].events &= ~events;
}

void
ctx_delstatus( btContext *ctx, int fd) {
    /* free up the status slot */
    int sid;
    int i;

    DIE_UNLESS(fd>=0 && fd<SOCKID_MAX);

    sid = ctx->statmap[fd];
    for (i=sid; i<ctx->nstatus; i++) {
        ctx->status[i] = ctx->status[i+1];
    }
    ctx->nstatus--;
    for (i=0; i<SOCKID_MAX; i++) {
	if (ctx->statmap[i] > sid) ctx->statmap[i]--;
    }
}

void
ctx_fixtmp( btContext *ctx, int fd) {
    int statblock;
    
    DIE_UNLESS(fd>=0 && fd<SOCKID_MAX);

    statblock = ctx->statmap[ TMPLOC];

    DIE_UNLESS(statblock >= 0 && statblock < MAXCONN);
    DIE_UNLESS(ctx->status[ statblock].fd == TMPLOC);

    /* relink the status block to the statmap */
    ctx->statmap[ fd] = statblock;
    ctx->status[ statblock].fd = fd;
    ctx->statmap[ TMPLOC] = -1;
}

int ctx_register( struct btContext *ctx, unsigned download)
    /*
     * Contact the tracker and update it on our status.  Also
     * add any new peers that the tracker reports back to us.
     */
{
    btDownload *dl=ctx->downloads[download];
    btObject *resp;

    DIE_UNLESS(download<ctx->downloadcount);
    /* contact tracker */
    resp = btrequest( 
	    dl->url, ctx->myid, ctx->mykey, dl->infohash, ctx->listenport, 
	    dl->fileset.dl, dl->fileset.ul * ctx->ulfactor, dl->fileset.left, "started"
	);
    if(!resp)
        return -EAGAIN;
#if 0
    btObject_dump( 0, resp);
#endif
    btresponse( ctx, download, resp);
    btObject_destroy( resp);
    return 0;
}

int ctx_startserver( btContext *ctx) {
    struct sockaddr_in sin;

    /* open server socket */
    ctx->ss = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP);
#if 0
    reuse = 1;
    if (setsockopt( ctx->ss, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
        bts_perror(errno, "setsockopt"); abort();
    }
#endif
    for ( ctx->listenport = MIN_PORT;
          ctx->listenport <=MAX_PORT; 
          ctx->listenport++) 
    {
	sin.sin_family = AF_INET;
	sin.sin_port = htons( ctx->listenport);
	sin.sin_addr.s_addr = INADDR_ANY;
	if (!bind( ctx->ss, (struct sockaddr *)&sin, sizeof(sin))) { 
            break;
	}
    }
    if (ctx->listenport > MAX_PORT) {
        bts_perror(errno, "bind"); abort();
    }
    if (listen( ctx->ss, 10)) { bts_perror(errno, "listen"); abort(); }

    /* setup for select */
    ctx_addstatus( ctx, ctx->ss);
    ctx_setevents( ctx, ctx->ss, POLLIN);
    return 0;    
}

int ctx_shutdown( btContext *ctx, unsigned download) {
    int result;
    btString *err;
    btObject *resp;
    btDownload *dl=ctx->downloads[download];

    DIE_UNLESS(download<ctx->downloadcount);
    /* contact tracker */
    resp = btrequest( 
	    dl->url, ctx->myid, ctx->mykey, dl->infohash, ctx->listenport, 
	    dl->fileset.dl, dl->fileset.ul * ctx->ulfactor,
	    dl->fileset.left, "stopped"
	);
#if 0
    btObject_dump( 0, resp);
#endif
    if(resp) {
	err = BTSTRING(btObject_val( resp, "failure reason"));
	if (err) {
	    printf("Error in shutdown from tracker: %s\n", err->buf);
	    result=-1;
	} else {
	    printf("Tracker shutdown complete\n");
	    result=0;
	}
	btObject_destroy( resp);
    } else {
        printf("Failed to tell tracker we've stopped\n");
        result=-1;
    }
    return result; 
}

int ctx_complete( btContext *ctx, unsigned download) {
    btString *err;
    btObject *resp;
    btDownload *dl=ctx->downloads[download];

    DIE_UNLESS(download<ctx->downloadcount);

    dl->complete=1;
    if(dl->fileset.dl==0)	/* don't send complete if we seeded */
        return 0;

    /* contact tracker */
    resp = btrequest( 
	    dl->url, ctx->myid, ctx->mykey, dl->infohash, ctx->listenport, 
	    dl->fileset.dl, dl->fileset.ul * ctx->ulfactor, dl->fileset.left, "completed"
	);
    if(!resp) {
        printf("Failed to tell tracker we completed\n");
	return -EAGAIN;
    }
#if 0
    btObject_dump( 0, resp);
#endif
    err = BTSTRING(btObject_val( resp, "failure reason"));
    if (err) {
	printf("Error in complete from tracker: %s\n", err->buf);
	btObject_destroy( resp);
	return -1;
    } else {
	printf("Tracker notified of complete\n");
    }
    btObject_destroy( resp);
    return 0; 
}

int ctx_reregister( btContext *ctx, unsigned download) {
    btObject *resp;
    btDownload *dl=ctx->downloads[download];

    DIE_UNLESS(download<ctx->downloadcount);
    /* contact tracker */
    resp = btrequest( 
	    dl->url, ctx->myid, ctx->mykey, dl->infohash, ctx->listenport, 
	    dl->fileset.dl, dl->fileset.ul * ctx->ulfactor, dl->fileset.left, NULL
	);
    if(!resp)
        return -EAGAIN;
#if 0
    btObject_dump( 0, resp);
#endif
    btresponse( ctx, download, resp);
    btObject_destroy( resp);
    return 0; 
}

void ctx_exit( int exitcode, void *arg) {
    btContext *ctx=arg;
    int i=ctx->downloadcount;
    
    while (i) {
	ctx_shutdown( arg, --i);
    }
    btContext_destroy( arg);
}

int ctx_loadfile( btStream *bts, struct btContext *ctx, int assumeok) {
    btStream *infostr;
    btString *announce;
    btInteger *size;
    struct btstrbuf strbuf;
    btString *hashdata;
    btInteger *piecelen;
    int npieces;
    int igood=0;
    int i, dlid;
    btDownload *dl;

    /* Allocate the new download */
    for(dlid=0; dlid<ctx->downloadcount; dlid++)
      if(!ctx->downloads[dlid])
	break;
    if(dlid==ctx->downloadcount) {
      btDownload **l=btrealloc(ctx->downloads, sizeof(btDownload*)*++ctx->downloadcount);
      if(l==NULL) {
        --ctx->downloadcount;
	return -ENOMEM;
      }
      ctx->downloads=l;
    }
    dl=btcalloc(1, sizeof(btDownload));
    if(!dl)
      return -ENOMEM;

    /* load the metadata file */
    if (benc_get_object( bts, &dl->md)) {
      btfree(dl);
      return -EINVAL;
      /*DIE("Load metadata failed");*/
    }

    /* calculate infohash */
    infostr = bts_create_strstream( BTS_OUTPUT);
    benc_put_object( infostr, btObject_val( dl->md, "info"));
    strbuf = bts_get_buf( infostr);
    SHA1( strbuf.buf, strbuf.len, dl->infohash);
    bts_destroy( infostr);

    /* copy out url */
    announce = BTSTRING( btObject_val( dl->md, "announce"));
    if (!announce) DIE( "Bad metadata file: 'announce' missing.");
    dl->url = btmalloc(announce->len+1);
    memcpy(dl->url, announce->buf, announce->len);
    dl->url[announce->len] = 0;

    /* set up the fileset and */
    /* calculate download size */
    dl->fileset.tsize = 0;
    size = BTINTEGER( btObject_val( dl->md, "info/length"));
    hashdata = BTSTRING( btObject_val( dl->md, "info/pieces"));
    piecelen = BTINTEGER( btObject_val( dl->md, "info/piece length"));
    npieces = hashdata->len / SHA_DIGEST_LENGTH;
    btFileSet_create( &dl->fileset, npieces, (int)piecelen->ival, hashdata->buf);
    kBitSet_create( &dl->requested, npieces);
    kBitSet_create( &dl->interested, npieces);
    for (i=0; i<npieces; i++) bs_set( &dl->interested, i);

    if (size) {
	/* single file mode */
	btString *file = BTSTRING( btObject_val( dl->md, "info/name.utf-8"));
	file||(file=BTSTRING( btObject_val( dl->md, "info/name")));
	dl->fileset.tsize=size->ival;
	btFileSet_addfile( &dl->fileset, file->buf, dl->fileset.tsize);
    } else {
	/* directory mode */
	btList *files;
	btFileSet *fs = &dl->fileset;
	btString *dir;

	dir = BTSTRING( btObject_val( dl->md, "info/name.utf-8"));
	dir||(dir = BTSTRING( btObject_val( dl->md, "info/name")));
	files = BTLIST( btObject_val( dl->md, "info/files"));
	if (!files) DIE( "Bad metadata file: no files.");
	for (i=0; i<files->len; i++) {
	    btInteger *fsize;
	    btList *filepath;
	    kStringBuffer path;
	    int j;

	    /* get file size */
	    fsize = BTINTEGER( btObject_val( files->list[i], "length"));
	    dl->fileset.tsize += fsize->ival;

	    /* get file path */
	    kStringBuffer_create( &path);
	    sbcat( &path, dir->buf, dir->len);
	    filepath = BTLIST( btObject_val( files->list[i], "path.utf-8"));
	    filepath||(filepath=BTLIST( btObject_val( files->list[i], "path")));
	    for (j=0; j<filepath->len; j++) {
	        btString *el = BTSTRING( filepath->list[j]);
		sbputc( &path, '/');
		sbcat( &path, el->buf, el->len);
	    }

	    /* add the file */
	    btFileSet_addfile( fs, path.buf, fsize->ival);

	    /* clean up */
	    kStringBuffer_finit( &path);
	}
    }
    dl->fileset.left = dl->fileset.tsize;
    for (i=0; i<npieces; i++) {
	int ok;
	if (assumeok) { 
	    bs_set(&dl->fileset.completed, i);
	    ok = 1;
	} else {
	    ok = seg_review( &dl->fileset, i);
	}
	if (i%10==0 || i > npieces-5) {
	    printf("\r%d of %d completed (%d ok)", i, npieces, igood);
	    fflush(stdout);
	}
	if (ok < 0) {
	    if (errno == ENOENT) {
	        int ni;
	        btFile *f = seg_findFile( &dl->fileset, i);
		if (!f) {
		    printf("couldn't find block %d\n", i);
		    continue;
		}
		ni = (int)((f->start + f->len) / dl->fileset.blocksize);
		if (ni > i) {
		    i = ni;
#if 1
		    printf("Skipping %d blocks\n", ni-i);
#endif
		}
	    }
#if 0
	    bts_perror(errno, "seg_review");
#endif
	    continue;
	}
	if (ok) {
	    igood++;
	    dl->fileset.left -= seg_piecelen( &dl->fileset, i);
	    /* seg_review already sets completed */
	    /* TODO: for unordered resumes, allow seg_review to return real piece number */
	    bs_set( &dl->requested, i);
	    bs_clr( &dl->interested, i);
	}
    }
    printf("\n");
    printf("Total good pieces %d (%d%%)\n", igood, igood * 100 / npieces);
    printf("Total archive size %lld\n", dl->fileset.tsize);
    bs_dump( "completed", &dl->fileset.completed);
    ctx->downloads[dlid]=dl;
    return dlid;
}

