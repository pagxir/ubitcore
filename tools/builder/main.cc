/*
 * announce: tracker
 * announce-list: tracker-list
 * create date: time-now
 * comment: description
 * created by: name
 * info:
 * 		name: file name
 * 		piece length: piece length
 * 		pieces: sha1 check sum
 * 		files:
 * 				length: file length
 * 				md5sum: md5 check sum
 * 				path: path
 * 		length:
 * 				file length
 * 		md5sum: 
 * 				file md5 check sum
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include "sha1.h"
#include "maker.h"
#define TRY_FREE(buf) if (buf!=NULL){ free(buf); }


static int __piece_count = 0;
static char __piece_hash[1024*1024*4];

static int __buffer_offset = 0;
static char __piece_buffer[1024*1024*4];

void usage()
{
	printf("main -h?aldbkuon");
	printf("no more help\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	int i;
	int opt = -1;
	int count = 0;
	const char * torrent = "test5.torrent";
	const char * path = NULL;
   	set_announce("http://localhost:6969/announce");
   	set_create_date(time(NULL));
	set_comment("No comment");
	set_create_by("unkown user");
	set_piece_length(256*1024);
	set_info_name("driver");
	for (i=1; i<argc; i++){
		switch(opt){
			case -1:
				if (argv[i][0]=='-'){
					opt = argv[i][1];
				}else{
					path = argv[i];
					info_files_add(argv[i], argv[i]);
					count++;
				}
				break;
			case 'h':
			case '?':
				usage();
				opt = -1;
				break;
			case '-':
				printf("long option not support yet!\n");
				opt = -1;
				break;
			case 'a':
			   	set_announce(argv[i]);
				opt = -1;
				break;
			case 'l':
			   	announce_list_add(argv[i]);
				opt = -1;
				break;
			case 'd':
			   	set_create_date(atoi(argv[i]));
				opt = -1;
				break;
			case 'm':
				set_comment(argv[i]);
				opt = -1;
				break;
			case 'n':
				count++;
				set_info_name(argv[i]);
				opt = -1;
				break;
			case 'k':
				set_piece_length(atoi(argv[i]));
				opt = -1;
				break;
			case 'u':
				set_create_by(argv[i]);
				opt = -1;
				break;
			case 'o':
				torrent = argv[i];
				opt = -1;
				break;
			default:
				printf("bad option!\n");
				exit(0);
				break;
		}
	}
	if (count == 0){
		usage();
	}
	if (count == 1){
	   	set_info_single_file(path);
	}
	save_file(torrent);
    return 0;
}

enum { SINGLE_FILE, MULTI_FILE};
static int __file_mode = SINGLE_FILE;
static char *__single_announce = NULL;
int set_announce(const char *url)
{
   	TRY_FREE(__single_announce);
	__single_announce = strdup(url);
	return 0;
}

static std::vector<char *> __announce_list;
int announce_list_add(const char *url)
{
	__announce_list.push_back(strdup(url));
	return 0;
}

static time_t __create_date;
int set_create_date(time_t now)
{
	__create_date = now;
	return 0;
}

static char *__comment = NULL;
int set_comment(const char *description)
{
	TRY_FREE(__comment);
	__comment = strdup(description);
	return 0;
}

static char *__create_by = NULL;
int set_create_by(const char *auth)
{
	TRY_FREE(__create_by);
	__create_by = strdup(auth);
	return 0;
}

static char *__info_name = NULL;
int set_info_name(const char *name)
{
	TRY_FREE(__info_name);
	__info_name = strdup(name);
	return 0;
}

static char *__single_file = NULL;
int set_info_single_file(const char *path)
{
	TRY_FREE(__single_file);
	__single_file = strdup(path);
	__file_mode = SINGLE_FILE;
	return 0;
}

static int __piece_length = 256*1024;
int set_piece_length(int length)
{
	__piece_length = length;
	return 0;
}

struct srcfile{
	char *tgpath;
	char *srcpath;
};
static std::vector<struct srcfile> __info_files;
int info_files_add(const char *tgpath, const char *path)
{
	struct srcfile element;
	element.tgpath = strdup(tgpath);
	element.srcpath = strdup(path);
	__info_files.push_back(element);
	__file_mode = MULTI_FILE;
	return 0;
}

static FILE *__torrent_file = NULL;
int torrent_write(const char *buf, int len)
{
	fwrite(buf, len, 1, __torrent_file);
	return 0;
}

int torrent_str_write(const char *buf, int len)
{
	char digit[256];
	int count = sprintf(digit, "%d:", len);
	torrent_write(digit, count);
	torrent_write(buf, len);
	return 0;
}

int write_created_by()
{
	char name[] = "create by";
	torrent_str_write(name, strlen(name));
	torrent_str_write(__create_by, strlen(__create_by));
	return 0;
}

int write_comment()
{
	const char name[]="comment";
	torrent_str_write(name, strlen(name));
	torrent_str_write(__comment, strlen(__comment));
	return 0;
}

int write_announce()
{
	const char name[] = "announce";
   	torrent_str_write(name, strlen(name));
   	torrent_str_write(__single_announce, strlen(__single_announce));
	return 0;
}

int write_info_name()
{
	const char name[]="name";
	torrent_str_write(name, strlen(name));
	torrent_str_write(__info_name, strlen(__info_name));
	return 0;
}

int write_piece_length()
{
	const char name[]="piece length";
	torrent_str_write(name, strlen(name));
	char buf[256];
	int count = sprintf(buf, "i%de", __piece_length);
	torrent_write(buf, count);
	return 0;
}

int update_hash()
{
	if (__buffer_offset > 0){
	   	SHA1Hash((unsigned char*)(__piece_hash+__piece_count*20),
			   	(unsigned char*)__piece_buffer, __buffer_offset);
	   	__buffer_offset = 0;
	   	__piece_count++;
	}
	return 0;
}

int build_hash(const char *path)
{
	int count = 0;
	unsigned char degist[20];
	FILE *fp = fopen(path, "rb");
	assert(fp != NULL);
	while(feof(fp) == 0){
	   	int read_byte = fread(__piece_buffer+__buffer_offset, 
				1, __piece_length-__buffer_offset, fp);
	   	if (read_byte > 0){
		   	count += read_byte;
		   	__buffer_offset += read_byte;
	   	}
		if (__buffer_offset >= __piece_length){
			update_hash();
		}
	}
	fclose(fp);
	return count;
}

int write_sigle_file()
{
	const char name[]="length";
	torrent_str_write(name, strlen(name));
	char buf[256];
	int file_length = build_hash(__single_file);
	int count = sprintf(buf, "i%ue", file_length);
	torrent_write(buf, count);
	return 0;
}

int write_path_list(const char *path)
{
	int i;
	int count = 0;
	torrent_write("l", 1);
	for (i=0; path[i]; i++){
		if (path[i]=='/' || path[i]=='\\'){
			if (count > 0){
			   	torrent_str_write(path+i-count, count);
			}
			count = 0;
		}else{
			count++;
		}
	}
   	if (count > 0){
	   	torrent_str_write(path+i-count, count);
   	}
	torrent_write("e", 1);
	return 0;
}

int write_path(const char *path)
{
	const char name[] = "path";
	torrent_str_write(name, strlen(name));
	write_path_list(path);
	return 0;
}

int write_per_file(const char *tgpath, const char *srcpath)
{
	char buf[56];
   	int length = build_hash(srcpath);
	torrent_write("d", 1);
	const char name[]= "length";
	torrent_str_write(name, strlen(name));
   	int count = sprintf(buf, "i%ue", length);
   	torrent_write(buf, count);
   	write_path(tgpath);
	torrent_write("e", 1);
	return 0;
}

int write_files()
{ 
	int i;
	char buf[56];
	const char name[] = "files";
	torrent_str_write(name, strlen(name));
	torrent_write("l", 1);
	for(i=0; i<__info_files.size(); i++){
		char *tgpath = __info_files[i].tgpath;
		char *srcpath = __info_files[i].srcpath;
		write_per_file(tgpath, srcpath);
	}
	torrent_write("e", 1);
	return 0;
}

int write_pieces()
{
	const char name[]="pieces";
	torrent_str_write(name, strlen(name));
	update_hash();
	torrent_str_write(__piece_hash, __piece_count*20);
	return 0;
}

int write_info()
{
	const char info[] = "info";
	torrent_str_write(info, strlen(info));
	torrent_write("d", 1);
	write_info_name();
	if (__file_mode == SINGLE_FILE){
	   	write_sigle_file();
	}else{
		write_files();
	}
	write_piece_length();
	write_pieces();
	torrent_write("e", 1);
	return 0;
}

int save_file(const char *path)
{
	__torrent_file = fopen(path, "wb");
	torrent_write("d", 1);
	write_announce();
	write_announce_list();
	write_create_date();
	write_comment();
	write_created_by();
	write_info();
	torrent_write("e", 1);
	fclose(__torrent_file);
	return 0;
}
int write_create_date()
{
	char buf[256];
	const char name[]="create date";
	torrent_str_write(name, strlen(name));
	int count = sprintf(buf, "i%ue", __create_date);
	torrent_write(buf, count);
	return 0;
}

int write_announce_list_element(const char *url)
{
	torrent_write("l", 1);
	torrent_str_write(url, strlen(url));
	torrent_write("e", 1);
	return 0;
}

int write_announce_list()
{
	int i;
	const char name[]="announce-list";
	torrent_str_write(name, strlen(name));
	torrent_write("l", 1);
	for (i=0; i<__announce_list.size(); i++){
		write_announce_list_element(__announce_list[i]);
	}
	torrent_write("e", 1);
	return 0;
}
