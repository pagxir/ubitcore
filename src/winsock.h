#ifdef __WIN32__
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

#define FIONBIO 1011

inline int Sleep(int tm)
{
    return sleep(tm / 1000);
}

inline int closesocket(int fd)
{
    return close(fd);
}

inline int WSASetLastError(int code)
{
    errno = code;
    return 0;
}

inline int WSAGetLastError()
{
    return errno;
}

inline int ioctlsocket(int fd, int cmd, unsigned long *arg)
{
    int fflag = fcntl(fd, F_GETFL);
    fflag |= O_NONBLOCK;
    fcntl(fd, F_SETFL, fflag);
    return 0;
}
#endif
