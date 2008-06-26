#ifndef __BUTILS_H__
#define __BUTILS_H__
const unsigned char * get_info_hash();
const unsigned char * get_peer_ident();
void set_info_hash(unsigned char hash[20]);
void set_peer_ident(unsigned char ident[20]);
#endif
