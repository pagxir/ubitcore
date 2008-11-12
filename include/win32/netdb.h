#ifndef __NETDB_H__
#define __NETDB_H__
#include <windows.h>
#include <winsock.h>
typedef int socklen_t;
#undef  errno
#undef  EAGAIN
#define EINPROGRESS WSAEWOULDBLOCK
#define EAGAIN WSAEWOULDBLOCK
#define errno WSAGetLastError()
#define close closesocket
#define O_NONBLOCK 0
#define F_GETFL 0
#define F_SETFL 1
#define SIGPIPE 0

inline
int fcntl(int fd, int cmd)
{
	return 0;
}

inline
int fcntl(int fd, int cmd, int arg)
{
	u_long fflag;
	ioctlsocket(fd, FIONBIO, (u_long*)&fflag);
	return 0;
}

inline
int sleep(int sec)
{
	Sleep(sec*1000);
	return 0;
}
#endif
