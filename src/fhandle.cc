#include <stdio.h>
#include <assert.h>
#include <winsock.h>
#include <queue>
#include <time.h>

#include "fhandle.h"
#include "fsocket.h"

#define SEL 1982
#define ACT 2891
using namespace std;
static int __callArgc = 0;
static void *__callArgv = NULL;
static int (*__callAgain) (fHandle * handle, int argc, void *argv) = NULL;
static queue < fHandle * >__active_queue;
fHandle::fHandle()
{
    flags = SEL;
} int

fHandle::Wakeup()
{
    if (flags != ACT) {
        __active_queue.push(this);
        flags = ACT;
    }
    return 0;
}

int fHandle::RunActive()
{
    printf("Hello World\n");
    return 0;
}

int fHandle::Abort()
{
    //printf("aborted: %p = %d\n", this, WSAGetLastError());
    return 0;
}

int WouldCallAgain(fHandle * handle)
{
    assert(handle != NULL);
    if (__callAgain != NULL) {
        (*__callAgain) (handle, __callArgc, __callArgv);
    } else {
        handle->Abort();
    }
    return 0;
}

int ClearCallAgain()
{
    __callAgain = NULL;
    return 0;
}
static int __nextWakeup;
static queue < fHandle * >__TimerQueue;
static int WakeupAllTimer()
{
    int now = time(NULL);
    if (__TimerQueue.empty()) {
        return 3600000;
    }
    if (now < __nextWakeup) {
        return __nextWakeup - now;
    }
    while (!__TimerQueue.empty()) {
        __TimerQueue.front()->Wakeup();
        __TimerQueue.pop();
    }
    return 0;
}
static int CallAgainNextTime(fHandle * handle, int wakeupTime, void *argv)
{
    if (wakeupTime < time(NULL)) {
        handle->Wakeup();
        return 0;
    }
    if (wakeupTime < __nextWakeup || __TimerQueue.empty()) {
        __nextWakeup = wakeupTime;
    }
    __TimerQueue.push(handle);
    return 0;
}
int WaitTime(int wakeupTime)
{
    if (wakeupTime > time(NULL)) {
        SetCallAgainHandle(CallAgainNextTime, wakeupTime, NULL);
        return -1;
    }
    return 0;
}
int
SetCallAgainHandle(int (*callAgain) (fHandle *, int, void *), int argc,
                   void *argv)
{
    __callAgain = callAgain;
    __callArgc = argc;
    __callArgv = argv;
    return 0;
}
static fHandle MARKER;
int fHandle::getNextReady(fHandle ** pHandle)
{
    int error = 0;
    int timeout = -1;
    fHandle *item = &MARKER;
    timeout = WakeupAllTimer();
    if (!__active_queue.empty()) {
        item = __active_queue.front();
        __active_queue.pop();
    }
    if (item == &MARKER) {
#if 1
        fSocket::Select(__active_queue.empty()? timeout : 0);
#endif
        WakeupAllTimer();
        __active_queue.push(&MARKER);
        item = __active_queue.front();
        __active_queue.pop();
    }
    if (item == &MARKER) {
        perror("item!=&MARKER");
    }
    assert(item != &MARKER);
    *pHandle = item;
    item->flags = SEL;
    return error;
}
