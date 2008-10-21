#ifndef __BUTILS_H__
#define __BUTILS_H__
const unsigned char * get_info_hash();
int set_info_hash(unsigned char hash[20]);
int getclientid(char ident[20]);
int setclientid(char ident[20]);
#endif
