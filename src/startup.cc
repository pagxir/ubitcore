#include "config.h"

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif

#ifndef HAVE_SLEEP

int
sleep(int sec)
{
   //Sleep(sec*1000);
   return 0;
}
#endif

#ifdef HAVE_WINSOCK_H

class dllbind
{
public:
	dllbind();
	~dllbind();
};

dllbind::dllbind()
{
	WSADATA data;
	WSAStartup(0x101, &data);
}

dllbind::~dllbind()
{
	WSACleanup();
}

static dllbind __socket_library_startup;
#endif
