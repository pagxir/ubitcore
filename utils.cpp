#include <stdio.h>

#include "utils.h"

static int hex_code(int ch)
{
	int uch = (ch & 0xFF);
	if (uch >= '0' && uch <= '9')
		return uch - '0';
	if (uch >= 'a' && uch <= 'f')
		return uch - 'a' + 10;
	if (uch >= 'A' && uch <= 'F')
		return uch - 'A' + 10;
	return 0;
}

size_t hex_decode(const char * hex, void * buf, size_t len)
{
	int code;
	unsigned char * p;
	unsigned char * orig_buf;

   	p = (unsigned char *)buf;
   	orig_buf = (unsigned char *)buf;

	while (len > 0) {
		if (*hex == 0) break;
		code = hex_code(*hex++);
		if (*hex == 0) break;
		code = (code << 4) | hex_code(*hex++);
		*p++ = (unsigned char)code;
		len--;
	}

	return (p - orig_buf);
}

