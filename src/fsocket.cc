#include <stdio.h>
#include <assert.h>
#include <winsock.h>
#include <list>
#include <queue>

#include "fhandle.h"
#include "fsocket.h"

#define ENQUEUE 0x1982
#define DEQUEUE 0x2891

static std::queue<fSocket*> __block_queue;

fSocket::fSocket()
{
    file = -1;
	msg = "OK";
    sock_close = -1;
    flags = DEQUEUE;
    blkOptval = 0;
    connected = 0;
    passiveMode = 0;
    readHandle = NULL;
    writeHandle = NULL;
    exceptHandle = NULL;
}

int fSocket::getSockaddr(sockaddr * addr, int *len)
{
    if ((size_t) * len >= sizeof(address)) {
        *len = sizeof(address);
        memcpy(addr, &address, sizeof(address));
    }
    return sizeof(address);
}
int fSocket::setBlockopt(int block)
{
    int oldValue = !blkOptval;
    blkOptval = !block;
    if (file != -1) {
        ioctlsocket(file, FIONBIO, &blkOptval);
    }
    return oldValue;
}
int fSocket::Connect(sockaddr * addr, int addrsize)
{
    int error = -1;
    Close();
    rCount = 0;
    connected = 1;
    passiveMode = 0;
    sock_close = 0;
    file = socket(AF_INET, SOCK_STREAM, 0);
    ioctlsocket(file, FIONBIO, &blkOptval);
    error = connect(file, addr, addrsize);
    if (error == -1) {
        if (WSAGetLastError() != EINPROGRESS) {
            printf("err=%d %d\n", EINPROGRESS, WSAGetLastError());
		   	sock_close = 1;
            Close();
            return -1;
        }
#if 0
        SetWaitConnect(this, error);
#endif
        connected = 0;
    }
    address = *(sockaddr_in *) addr;
    return 0;
}

int fSocket::WaitRead(fHandle * handle)
{
    assert(handle != NULL);
    if (readHandle != NULL && handle != readHandle) {
		fprintf(stderr, "warn: WaitRead: %p %p!\n", readHandle, handle);
        readHandle->Wakeup();
    }
    readHandle = handle;
    Suppend();
    return 0;
}

int fSocket::WaitExcept(fHandle * handle)
{
    assert(handle != NULL);
    if (exceptHandle != NULL && exceptHandle != handle) {
		fprintf(stderr, "warn: WaitExcept: %p %p!\n", 
				handle, exceptHandle);
		display();
        exceptHandle->Wakeup();
    }
    if (writeHandle != NULL && exceptHandle != handle) {
		fprintf(stderr, "warn: WaitExcept1!\n");
        writeHandle->Wakeup();
    }
    writeHandle = NULL;
    exceptHandle = handle;
    Suppend();
    return 0;
}

int fSocket::isPassive()
{
    return passiveMode;
}

int fSocket::Close()
{
	sock_close++;
	msg = "Close";
    closesocket(file);
#define DEAD_CALL(readHandle) \
	if (readHandle != NULL){  \
		readHandle->Wakeup(); \
	   	readHandle = NULL;    \
	}
	DEAD_CALL(readHandle);
	DEAD_CALL(writeHandle);
	DEAD_CALL(exceptHandle);
#undef DEAD_CALL
    file = -1;
    return 0;
}

int fSocket::Connected()
{
    int error = -1;
    switch (connected) {
    case 0:
        WSASetLastError(EAGAIN);
        break;
    case 1:
        error = 0;
        break;
    case 2:
        return -1;
    default:
        break;
    }
    return SetWaitConnect(this, error);
}

int fSocket::WaitWrite(fHandle * handle)
{
    assert(handle != NULL);
    if (writeHandle != NULL && handle != writeHandle) {
		fprintf(stderr, "warn: WaitWrite!\n");
        writeHandle->Wakeup();
    }
    if (exceptHandle != NULL && handle != exceptHandle) {
		fprintf(stderr, "warn: WaitWrite2!\n");
        handle->Abort();
        return 0;
    }
    if (exceptHandle == NULL) {
        writeHandle = handle;
    }
    Suppend();
    return 0;
}
int fSocket::SetExcept(int code)
{
    if (code == 0) {
        connected = 1;
    } else if (code == 1) {
        connected = 2;
    }
    if (exceptHandle != NULL) {
        exceptHandle->Wakeup();
        exceptHandle = NULL;
    }
    return -(code > 1);
}
int fSocket::Send(char *buf, int len, int flag)
{
    if(file == -1){
		return -1;
	}
    int error = send(file, buf, len, flag);
    if (error==-1 && errno!=EAGAIN) {
		sock_close++;
		msg = "fSocket::Send";
    }
    return SetWaitWrite(this, error);
}
int fSocket::Recv(char *buf, int len, int flag)
{
    if(file == -1){
        return -1;
    }
    ioctlsocket(file, 0, NULL);
    int error = recv(file, buf, len, flag);
    if (error == -1) {
        if (errno != EAGAIN){
		   	sock_close++;
		   	msg = "fSocket::Send";
		}
    }
	if (error == 0){
		if (sock_close > 10){
			printf("fixme: find close socket bug!: %s\n", msg);
		}
		sock_close++;
		msg = "sockclose";
		printf("sockclose: %p\n", this);
	}
    return SetWaitRead(this, error);
}

int fSocket::Suppend()
{
    if (flags == DEQUEUE) {
        flags = ENQUEUE;
        __block_queue.push(this);
    }
    return 0;
}

int fSocket::GetPoll(fd_set * readfds, fd_set * writefds,
                     fd_set * exceptfds)
{
    int count = 0;
    if (exceptHandle != NULL) {
        assert(writeHandle == NULL);
        if (FD_ISSET(file, readfds)) {
            SetExcept(1);
            count++;
        } else if (FD_ISSET(file, writefds)) {
            SetExcept(0);
            count++;
        }
    } else if (writeHandle != NULL) {
        if (FD_ISSET(file, writefds)) {
            writeHandle->Wakeup();
            writeHandle = NULL;
            count++;
        }
    }
    if (readHandle != NULL && FD_ISSET(file, readfds)) {
        readHandle->Wakeup();
        readHandle = NULL;
        count++;
    }
    return count;
}

int fSocket::SetPoll(fd_set * readfds, fd_set * writefds,
                     fd_set * exceptfds)
{
    int count = 0;
    if (file == -1) {
        readHandle && readHandle->Wakeup();
        writeHandle && writeHandle->Wakeup();
        exceptHandle && exceptHandle->Wakeup();
        readHandle = writeHandle = exceptHandle = NULL;
        flags = DEQUEUE;
        return 0;
    }
	assert(sock_close != -1);
	if (sock_close > 0){
		sock_close++;
		if (sock_close > 10){
		   	printf("fixme: bug: %s\n", msg);
		}
	}
    if (readHandle != NULL) {
        FD_SET(file, readfds);
        count++;
    }
    if (exceptHandle != NULL) {
        FD_SET(file, readfds);
        count++;
    }
    if (writeHandle != NULL || exceptHandle != NULL) {
        FD_SET(file, writefds);
        count++;
    }
    if (count == 0) {
        flags = DEQUEUE;
    }
    return count;
}
int fSocket::Select(int timeout)
{
    int count = 0;
    int wait = timeout;
    timeval waitTime;
    fSocket *each_block;
    fd_set readfds, writefds, exceptfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    std::queue<fSocket*>active_queue;
    fSocket MARKER;
    __block_queue.push(&MARKER);
    each_block = __block_queue.front();
    __block_queue.pop();
    while (each_block != &MARKER) {
        if (each_block->SetPoll(&readfds, &writefds, &exceptfds) > 0) {
            active_queue.push(each_block);
            __block_queue.push(each_block);
            wait = 0;
        }
        each_block = __block_queue.front();
        __block_queue.pop();
    }
    waitTime.tv_sec = timeout;
    waitTime.tv_usec = 0;
    if (wait > 0) {
        Sleep(wait * 1000);
    } else {
        count = select(200, &readfds, &writefds, &exceptfds, &waitTime);
    }
    int index = 0;
    while (!active_queue.empty() && index < count) {
        fSocket *each_block = active_queue.front();
        active_queue.pop();
        index += each_block->GetPoll(&readfds, &writefds, &exceptfds);
    }
    return count;
}
static int CallAgainNextRead(fHandle * handle, int argc, void *argv)
{
    fSocket *file = (fSocket *) argv;
    file->WaitRead(handle);
    return 0;
}
int SetWaitRead(void *file, int code)
{
    if (code == -1 && WSAGetLastError() == EAGAIN) {
        SetCallAgainHandle(CallAgainNextRead, 1, file);
    }
    return code;
}
static int CallAgainNextWrite(fHandle * handle, int argc, void *argv)
{
    fSocket *file = (fSocket *) argv;
    file->WaitWrite(handle);
    return 0;
}
int SetWaitWrite(void *file, int code)
{
    if (code == -1 && WSAGetLastError() == EAGAIN) {
        SetCallAgainHandle(CallAgainNextWrite, 1, file);
    }
    return code;
}
static int CallAgainNextConnect(fHandle * handle, int argc, void *argv)
{
    fSocket *file = (fSocket *) argv;
    file->WaitExcept(handle);
    return 0;
}
int SetWaitConnect(void *file, int code)
{
    if (code == -1 && WSAGetLastError() == EAGAIN) {
        SetCallAgainHandle(CallAgainNextConnect, 1, file);
    }
    return code;
}
