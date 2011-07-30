#ifndef _UTILS_H_
#define _UTILS_H_
void hex_dump(const char *buf, size_t len);
char *hex_encode(char *out, const void *dat, size_t len);
size_t hex_decode(const char *hex, void *buf, size_t len);
#endif

