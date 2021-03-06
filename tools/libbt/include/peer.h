#ifndef __PEER__H
#define __PEER__H
#include "stream.h"
#include "bitset.h"
#include "segmenter.h"
#if WIN32
#   include <winsock2.h>
#endif

#define IDSIZE 20
#define QUEUESIZE 30
#define MAXREQUEST (64 * 1024) /* maximum request to send */
#define MAXMESSAGE (MAXREQUEST + 100) /* maximum message size we can receive */

typedef enum { PEER_INIT, PEER_OUTGOING, PEER_INCOMING, PEER_GOOD, PEER_ERROR } peerstates;

struct btContext;
typedef struct btRequest {
    int block;
    int offset;
    int length;
} btRequest;

typedef struct btRequestQueue {
    int head;
    int tail;
    btRequest req[QUEUESIZE];
} btRequestQueue;

typedef struct ptPeerStatus {
    time_t send_time;	/* time of last unchoke */
    time_t total_time;	/* total time sending */
    unsigned int choked:1;	/* peer isn't sending */
    unsigned int snubbed:1;	/* no data from peer */
    unsigned int interested:1;	/* peer wants something */
    unsigned int unreachable:1;	/* can't connect to peer */
    unsigned int complete:1;    /* peer is complete */
} btPeerStatus;


typedef struct btPeer {
    /*
     * INIT - Initialized
     * CONNECT - Connected
     * GOOD - Handshake exchanged
     */
	peerstates state;
    int download;		/* download in btContext */
    kStream ios;
    char id[IDSIZE];
    struct in_addr ip;
    int port;
    int time;
    kBitSet blocks;		/* blocks available from this peer */
    btPeerStatus remote;	/* remote state information */
    btPeerStatus local;		/* local state for this peer */
    char *error;

    btPartialPiece *currentPiece;
    btRequestQueue inqueue;	/* request queue for incoming data */
    btRequestQueue queue;	/* outbound requests */
    time_t lastreceived;	/* time of last receipt of a piece */
} btPeer;

typedef struct btPeerset {
    int len;
    int interestedpeers;	/* peers interested in my blocks */
    int incomplete;             /* incomplete peers */
    struct btPeer** peer;
} btPeerset;

btPeerset *btPeerset_create( btPeerset *buf);
struct btPeer*  peer_add( struct btContext *ctx, unsigned download, char *id, struct in_addr *ip, int port) ;
int peer_connect_complete( struct btContext *ctx, struct btPeer *p);
int peer_send_handshake( struct btContext *ctx, struct btPeer *p);
int peer_send_bitfield( struct btContext *ctx, struct btPeer *p);

int peer_answer( struct btContext *ctx, int sock) ;

void peer_favorites( struct btContext *ctx, btPeerset *pset);

void peer_shutdown( struct btContext *ctx, btPeer* peer, char *error) ;

int peer_recv_message( struct btContext *ctx, btPeer* peer) ;
btPartialPiece* peer_assign_block( struct btContext *ctx, btPeer* peer) ;
int peer_send_request( struct btContext *ctx, btPeer* peer) ;
int update_interested( struct btContext *ctx, btPeer* peer) ;


int peer_process_queue( btFileSet *fs, btPeer *p) ;
void peer_dump( btPeerset *pset);
void peer_summary( btPeerset *pset);

int peer_allcomplete( btPeerset *pset);

#endif


