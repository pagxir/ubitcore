#include <windows.h>
#include <winsock.h>

inline int
sleep(int t)
{
	Sleep(t*1000);
	return 0;
}

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

static dllbind __startup_obj;
