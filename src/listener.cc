#include <stdio.h>
#include <time.h>
#include <winsock.h>
#include <queue>
#include <assert.h>
#include "fhandle.h"
#include "fsocket.h"
#include "upeer.h"
#include "netmgr.h"
#include "session.h"
#include "peer.h"
#include "tracke.h"
#include "listener.h"
static int __localPort;
class listenSocket:public fSocket  {
public:
    listenSocket();
    int Accept(sockaddr * addr, int *addrsize);
    int Bind(sockaddr * addr, int addrsize);
};

listenSocket::listenSocket()
{
    file = -1;
} 

int listenSocket::Bind(sockaddr * addr, int addrsize)
{
    int error = -1;
    int cflag = 1;
    sockaddr_in localaddr;
    int addrlen = sizeof(localaddr);
    file = socket(AF_INET, SOCK_STREAM, 0);
    if (file == -1) {
        goto fail_exit;
    }
    setsockopt(file, SOL_SOCKET, SO_REUSEADDR,
               (char*)&cflag, sizeof(cflag));
    error = bind(file, addr, addrsize);
    if (error == -1) {
        goto fail_exit;
    }
    error = listen(file, 5);
    if (error == -1) {
        goto fail_exit;
    }
    setBlockopt(0);
    error =
        getsockname(file, (sockaddr *) & localaddr,
                    (socklen_t*) &addrlen);
    if (error == -1) {
        goto fail_exit;
    }
    __localPort = htons(localaddr.sin_port);
    printf("listen port: %d\n", __localPort);
	sock_close = 0;
	msg = "listen";
    error = 0;
fail_exit:
    if (error == -1) {
        Close();
    }
    return error;
}
int listenSocket::Accept(sockaddr * addr, int *addrsize)
{
    int error =::accept(file, addr, (socklen_t*) addrsize);
    return SetWaitRead(this, error);
}

class Listener:public fHandle  {
public:
    Listener();
    virtual ~ Listener();
    int RunActive();
private:
    listenSocket file;
    int state;
};

Listener::Listener()
{
    state = 0;
} Listener::~Listener()
{
    file.Close();
} int

Listener::RunActive()
{
    int error = 0;
    int nextState = state;
    sockaddr_in addr;
    int addrlen = sizeof(addr);
    while (error != -1) {
        state = nextState++;
        switch (state) {
        case 0:
            addr.sin_family = AF_INET;
            addr.sin_port = htons(6400);
            addr.sin_addr.s_addr = INADDR_ANY;
            error = file.Bind((sockaddr *) & addr, sizeof(addr));
            file.setBlockopt(0);
            break;
        case 1:
            error = file.Accept((sockaddr *) & addr, &addrlen);
            break;
        default:
            SetActivePeer(error, &addr, addrlen);
            nextState = 1;
            break;
        }
    }
    return error;
}
static Listener __listener;
int WakeupListener()
{
    return __listener.Wakeup();
}
int getListenPort()
{
    return __localPort;
}


