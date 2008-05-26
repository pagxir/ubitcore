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

/*
 * 开始算法处理工作!
 *
 */

int PeerSocket::Attach(int fild, sockaddr_in *addr, int addrlen, int wait)
{
    assert(file == -1);
    passiveMode = 0;
    switch (wait) {
    case 1:
        passiveMode = 1;
    case 0:
        connected = 1;
        break;
    case -1:
        connected = 0;
        break;
    default:
        break;
    }
    file = fild;
    address = *addr;

    rCount = 0;
    tOffset = 0;
    tLength = 0;

    loff = 0;
    offsetIn = 0;
    lengthIn = 4;
	sock_close = 0;

    writeHandle = NULL;
#if 0
    SetWaitConnect(this, wait);
#endif
    return 0;
}

int PeerSocket::RunActive()
{
    int error = 0;
    while (error!=-1 && tOffset<tLength) {
        tOffset += error;
        error = Send(tBuffer+tOffset, tLength-tOffset, 0);
    }
    if (error!=-1 && writeHandle!=NULL) {
        writeHandle->Wakeup();
        writeHandle = NULL;
    }
    return error;
}

int PeerSocket::waitPacket(fHandle *handle)
{
    if (writeHandle!=NULL && writeHandle!=handle) {
        writeHandle->Wakeup();
    }
    writeHandle = handle;
    return 0;
}

static int callAgainNextFlush(fHandle *handle, int argc, void *argv)
{
    PeerSocket *peerSocket = (PeerSocket*)argv;
    return peerSocket->waitPacket(handle);
}

static int setWaitFlush(int code, void *handle)
{
    if (code==-1 && WSAGetLastError()==10035) {
        SetCallAgainHandle(callAgainNextFlush, 1, handle);
    }
    return code;
}

int PeerSocket::SendPacket(char buffer[], int length)
{
    int result = 0;
    int tmp = htonl(length);
    char *buf = new char[length+4];
    assert(buf != NULL);
    memcpy(buf, (char*)&tmp, 4);
    memcpy(buf+4, buffer, length);
    result = SendAll(buf, length+4);
    delete[] buf;
    return result;
}


int PeerSocket::SendAll(char buffer[], int length)
{
    int error = 0;
	if(file == -1){
		errno = 0;
		return -1;
	}
    assert(length < (int)sizeof(tBuffer));
    while (error!=-1 && tOffset<tLength) {
        tOffset += error;
        error = send(file, tBuffer+tOffset, tLength-tOffset, 0);
    }
    if (error==-1) {
		if (error==-1 && errno != EAGAIN){
			sock_close++;
			msg = "SendAll";
		}
        return setWaitFlush(error, this);
    }
    int count = 0;
    int offset = 0;
    while (count!=-1 && offset<length) {
        offset += count;
        count = send(file, buffer+offset, length-offset, 0);
		if (count==-1 && errno != EAGAIN){
			sock_close++;
			msg = "SendAll1";
		}
    }
    if (count==-1 && WSAGetLastError()==EAGAIN) {
        tOffset = 0;
        tLength = length-offset;
        memcpy(tBuffer, buffer+offset, tLength);
        Wakeup();
    }
    return length;
}

int PeerSocket::RecvPacket(char buffer[], int length)
{
    int i;
    int error = 0;

again:
    while (loff < 4) {
        error = Recv(packetIn, 4-loff);
        if (error <= 0) {
            return error;
        }
        for (i=0; i<error; i++) {
            lengthIn <<= 8;
            lengthIn |= (packetIn[i]&0xff);
        }
        offsetIn = 0;
        loff += error;
    }

    if (lengthIn == 0) {
        loff = 0;
        goto again;
    }

    //printf("rCount: %d\n", rCount);
    assert(lengthIn < (int)sizeof(packetIn));
    while (offsetIn < lengthIn) {
        error = Recv(packetIn+offsetIn, lengthIn-offsetIn);
        if (error <= 0) {
            return error;
        }
        offsetIn += error;
    }
    loff = 0;
    assert(lengthIn < length);
    memcpy(buffer, packetIn, lengthIn);
    return lengthIn;
}

int PeerSocket::display()
{
	printf("o=%p, w=%p\n", owner, writer);
	return 0;
}
