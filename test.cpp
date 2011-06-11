#include <stdio.h>
#include <assert.h>
#include <winsock.h>

#include "btcodec.h"

int main(int argc, char *argv[])
{
#if 0
	FILE *fp = fopen("C:\\2kad_log.log", "rb");
	char buf[8192];
	int count = fread(buf, 1, sizeof(buf),  fp);
	fclose(fp);

	btcodec codec;
	codec.load(buf, count);
#endif
	return 0;
}

