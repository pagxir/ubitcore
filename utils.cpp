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

size_t hex_decode(const char *hex, void *buf, size_t len)
{
	int code;
	unsigned char *p;
	unsigned char *orig_buf;

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

void hex_dump(const char *buf, size_t len)
{
	int i, j, c;

	for (i = 0; i < len; i += 16) {
		c = (len - i < 16? len - i: 16);
	   	fprintf(stderr, "%04x: ", i);
		for (j = 0; j < c; j++)
			fprintf(stderr, "%02X ", buf[i + j] & 0xFF); 
		fprintf(stderr, "%02X\n", buf[i + j] & 0xFF);
	}
}

char *hex_encode(char *out, const void *dat, size_t len)
{
	char *outp = (char *)out;
	const char *datp = (const char *)dat;

	while (len-- > 0) {
		sprintf(outp, "%02X", 0xFF & *datp);
		outp += 2;
		datp++;
	}

	return out;
}

