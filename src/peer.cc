#include <stdio.h>
#include <time.h>
#include <queue>
#include <winsock.h>
#include <assert.h>
#include "handshake.h"
#include "fhandle.h"
#include "fsocket.h"
#include "upeer.h"
#include "netmgr.h"
#include "session.h"
#include "peer.h"
#include "tracke.h"
#include "listener.h"
#include "zpiece.h"
#include "speedup.h"

PeerWriteProcess::PeerWriteProcess(PeerProcess * handle)
{
    peerProcess = handle;
}

int PeerWriteProcess::RunActive()
{
    return peerProcess->writeNextPacket();
}

extern int __piece_count;
char __bitField[10240];
char __bitTransfer[10240];
static std::queue < fHandle * >__queueProcess;
static int CallAgainNextPacket(fHandle * handle, int argc, void *argv)
{
    __queueProcess.push(handle);
    return 0;
}

static int WakeupNextPacket()
{
    while (!__queueProcess.empty()) {
        __queueProcess.front()->Wakeup();
        __queueProcess.pop();
    }
    return 0;
}

PeerProcess::PeerProcess():writeProcess(this)
{
    fildes = -1;
    state = 0;
    queued = 0;
    goodPeer = 0;
	caller = NULL;
    bitField = new char[4+(__piece_count/8)];
    haveField = new char[4+(__piece_count/8)];
	peerSocket.owner = this;
	peerSocket.writer = &writeProcess;
}

int PeerProcess::Abort()
{
    peerSocket.Close();
    state = 0;
    return 0;
}

int PeerProcess::saveAddress(char *buffer, int length)
{
    if (length == sizeof(sockaddr_in)) {
        memcpy(&peerName, buffer, length);
    }
    return length;
}

extern int __pieceLength;
int sendRequest(PeerSocket * peerSocket, int piece, int start, int count)
{
    char buffer[0xd];
    long *ptr = (long *) (buffer + 1);
    buffer[0] = 0x6;
    ptr[0] = htonl(piece);
    ptr[1] = htonl(start);
    ptr[2] = htonl(count);
    fprintf(stderr, "\rsending request");
    return peerSocket->SendPacket(buffer, 0xd);
}

static int sendMessageInsterest(PeerSocket * peerSocket, int cmd)
{
    char buffer[0x1];
    buffer[0] = cmd;
    return peerSocket->SendPacket(buffer, 0x1);
}

static int sendMessageHave(PeerSocket * peerSocket, int piece)
{
    char buffer[0x5];
    long *ptr = (long *) (buffer + 1);
    buffer[0] = 0x4;
    ptr[0] = htonl(piece);
    return peerSocket->SendPacket(buffer, 0x5);
}

static int sendSegment(PeerSocket * pSocket,
           request_t * request, const char data[])
{
    int len = htonl(request->q_length);
    char buffer[256 * 1024];
    buffer[0] = 0x7;
    memcpy(buffer+1, request, 8);
    memcpy(buffer+9, data, len);
    return pSocket->SendPacket(buffer, len+9);
}

int add_transfer_peer(PeerProcess *peer);

int PeerProcess::writeNextPacket()
{
    int i;
    for (i = 0; i < __piece_count; i++) {
        if (!bitfield_test(haveField, i) && bitfield_test(__bitField, i)) {
            if (-1 == sendMessageHave(&peerSocket, i)) {
                return -1;
            }
            bitfield_set(haveField, i);
        }
    }
    if (local_chocked != local_next_chocked) {
        /*chocking=0: local chock this peer!*/
        /*chocking=1: local unchock this peer!*/
        if (-1 == sendMessageInsterest(&peerSocket, local_chocked)) {
            return -1;
        }
        printf("%s %p\n", local_chocking?"chock":"unchock", this);
        if (remote_insterested==1 && local_chocking==0) {
            /* sart remote rate timer */
            start_remote_timer();
        }
        if (local_chocking==1) {
            /* stop remote rate timer */
            stop_remote_timer();
        }
        local_chocked = local_next_chocked;
    }
    if (queued<512*1024 && 
            (remote_chocked==0||local_interested!=local_interesting)) {
        while (queued<1024*1024 && local_interesting==1) {
            segment_t segment;
            int error = assign_segment(&segment, bitField);
            if (error == -1) {
                local_interesting = 0;
                stop_local_timer();
            } else {
                local_interesting = 1;
                if (remote_chocked==0) {
                    start_local_timer();
                }
            }
            if (local_interested != local_interesting) {
                if (-1 == sendMessageInsterest(&peerSocket, 
                            local_interested+2)) {
                    return -1;
                }
                printf("%s: %p\n", local_interesting?"interesting":"not interesting",this);
                local_interested = local_interesting;
            }
            if (remote_chocked == 1) {
                reassign_segment(bitField);
                break;
            }
            if (-1 == sendRequest(&peerSocket, segment.seg_piece,
                           segment.seg_offset, segment.seg_length)) {
                return -1;
            }
            queued += segment.seg_length;
        }
    }
    while ((local_chocking==0||add_transfer_peer(this))&&!mRequests.empty()) {
        
add_transfer_peer(this);
        char buffer[256 * 1024];
        int piece, length, start;
        request_t request = mRequests.front();
        length = htonl(request.q_length);
        piece = htonl(request.q_piece);
        start = htonl(request.q_start);
        int len = segbuf_read(piece, start, buffer, length);
        if (len == -1) {
            mRequests.pop();
            continue;
        }
        if (-1 == sendSegment(&peerSocket, &request, buffer)) {
            return -1;
        }
        fprintf(stderr, "\rsend buf: piece(%d) start(%d) length(%d)",
                   piece, start, length);
        mRequests.pop();
    }
    SetCallAgainHandle(CallAgainNextPacket, 1, this);
    return -1;
}
int get_hand_shake(char *buf, int len);
int PeerProcess::dumpHandShake(char *buf, int len)
{
    if (len == 68) {
        if (buf[0] != 0x13) {
            return -1;
        }
        if (strncmp(buf + 1, "BitTorrent protocol", 0x13)) {
            return -1;
        }
#if 1
        int i;
        hand_shake_time = time(NULL);
        //static HandShakeManager hsManager;
        printf(" en peer_id=");
        for (i = 0; i < 20; i++) {
            printf("%02x", buf[48 + i] & 0xff);
        }
        //hsManager.AddPeerId(buf+48);
        memcpy(peer_ident, buf+48, 20);
        try_add_peer_id(this);
        printf("\n");

#endif
        return 0;
    }
    return -1;
}

int PeerProcess::IsActive()
{
    if (state==5 || state==6) {
        return 1;
    }
    return 0;
}

int PeerProcess::Chock()
{
    local_chocking = 1;
    if (mRequests.size()>0) {
        local_next_chocked = 1;
        while (mRequests.empty()) {
            mRequests.pop();
        }
    }
    writeProcess.Wakeup();
    return 0;
}

int PeerProcess::Unchock()
{
    local_chocking = 0;
    local_next_chocked = 0;
    writeProcess.Wakeup();
    return 0;
}

int PeerProcess::dumpPeerInfo()
{
    if (goodPeer == 1) {
        if (peer_ident[0]==0x2d
                && peer_ident[1]==0x58
                && peer_ident[2]==0x4c) {
            printf("迅雷: %d\n", time(NULL)-hand_shake_time);
        } else {
            printf("unkown: %d\n", time(NULL)-hand_shake_time);
        }
        //printf("remote chock: %d\n", chocked);
        //printf("remote insterested: %d\n", insterested);
    }
    return 0;
}

void PeerProcess::cancel_request(request_t *request)
{
    int count = mRequests.size();

    while (!mRequests.empty() && count>0) {
        request_t q = mRequests.front();
        mRequests.pop();
        count--;
        if (q.q_length!=request->q_length) {
            mRequests.push(q);
        } else if (q.q_start!=request->q_start) {
            mRequests.push(q);
        } else if (q.q_piece!=request->q_piece) {
            mRequests.push(q);
        }
    }
    printf("cancel request!\n");
}

extern int __piece_size;
extern int __last_piece_size;
int PeerProcess::totalReceive = 0;
int PeerProcess::RunActive()
{
    int i;
    int len;
    int wait;
    int error = 0;
    struct sockaddr_in iaddr;
    char buffer[819200];
    int nextState = state;
    while (error != -1) {
        state = nextState++;
        switch (state) {
        case 0:
            wait = 1;
            if (fildes == -1){
                fildes = socket(AF_INET, SOCK_STREAM, 0);
                ioctlsocket(fildes, FIONBIO, NULL);
                iaddr.sin_family = AF_INET;
                iaddr.sin_port = port;
                iaddr.sin_addr.s_addr = address;
                wait = connect(fildes, (sockaddr*)&iaddr, sizeof(iaddr));
                if (wait==-1 && WSAGetLastError()!=EINPROGRESS){
                    perror("connect");
                    closesocket(fildes);
                    fildes = -1;
                    return 0;
                }
            }
            peerSocket.Attach(fildes, &iaddr, sizeof(iaddr), wait);
            fildes = -1;
            break;
        case 1:
            connectTime=0;
            queued = 0;
            local_chocked = 0;
            local_chocking = 1;
            local_next_chocked = 0;
            local_interested = 0;
            local_interesting = 0;
            remote_chocked = 1;
            remote_insterested = 0;
            readCount = 0;
            lastRead = 0;
            local_total_time = 0;
            local_send_time = 0;
            remote_total_time = 0;
            remote_send_time = 0;

            timeRing = 0;
            for (i=0; i<128; i++) {
                timeUsed[i] = 0;
                readBytes[i] = 0;
            }
            memset(bitField, 0, 
                    (__piece_count+(__last_piece_size>0)+7)/8);
            error = peerSocket.Connected();
            break;
        case 2:
            if (peerSocket.isPassive()) {
                error = peerSocket.Recv(buffer, 68);
                if (error != -1 && dumpHandShake(buffer, error) == -1) {
                    nextState = 9;
                }
            } else {
                len = get_hand_shake(buffer, sizeof(buffer));
                error = peerSocket.Send(buffer, len);
            }
            break;
        case 3:
            if (!peerSocket.isPassive()) {
                error = peerSocket.Recv(buffer, 68);
                if (error != -1 && dumpHandShake(buffer, error) == -1) {
                    nextState = 9;
                }
            } else {
                len = get_hand_shake(buffer, sizeof(buffer));
                error = peerSocket.Send(buffer, len);
            }
            break;
        case 4:
#if 1
            len = __last_piece_size?__piece_count+1:__piece_count;
            len = (len+7)/8;
            buffer[0] = 0x5;
            memcpy(buffer+1, __bitField, len);
            memcpy(haveField, __bitField, len);
            if (-1 != peerSocket.SendPacket(buffer, len + 1)) {
                printf("bit field send ok!\n");
            }
            local_chocked = 1;
            Unchock();
            addToChockSched(this);
#endif
            break;
        case 5:
            error = peerSocket.RecvPacket(buffer, sizeof(buffer));
            break;
        case 6:
            goodPeer = 1;
            if (error > 0) {
                int index = 0;
                int start = 0;
                request_t request;
                switch (buffer[0]) {
                case 0:
                    //printf("remote chock\n");
                    queued = 0;
                    remote_chocked = 1;
                    reassign_segment(bitField);
                    stop_local_timer();
                    break;
                case 1:
                    printf("remote unchock\n");
                    remote_chocked = 0;
                    writeProcess.Wakeup();
                    if (local_interesting) {
                        start_local_timer();
                    }
                    break;
                case 2:
                    remote_insterested = 1;
                    if (local_chocking == 0) {
                        start_remote_timer();
                    }
                    //printf("insterested\n");
                    break;
                case 3:
                    remote_insterested = 0;
                    stop_remote_timer();
                    //printf("not interested\n");
                    break;
                case 6:
                    assert(error == 0xd);
                    //fprintf(stderr, "request: %p\n", this);
                    memcpy(&request, buffer+1, sizeof(request));
                    if (ntohl(request.q_piece)<0 
                            || ntohl(request.q_piece)>=__piece_count) {
                        Abort();
                    } else if (ntohl(request.q_length)>(1<<17) 
                            || ntohl(request.q_length)<16*1024) {
                        Abort();
                    } else if (ntohl(request.q_start)<0
                               || ntohl(request.q_start)+ntohl(request.q_length)>__piece_size) {
                           Abort();
                    } else {
                        if (local_chocking==0 || add_transfer_peer(this)){
add_transfer_peer(this);
                       mRequests.push(request);
                       writeProcess.Wakeup();
                        }else{
                       local_next_chocked = local_chocking;
            }
                    }
                    break;
                case 7:
                    index = ntohl(*(long *) (buffer + 1));
                    start = ntohl(((long *) (buffer + 1))[1]);
                    queued -= (error-9);
                    totalReceive += (error-9);
                    readCount += (error-9);
                    readBytes[timeRing] += (error-9);
                    fprintf(stderr, "\rpiece: %5d %6d %7d %p", index, start,
                            getSpeed(), this);
                    segbuf_write(index, start, buffer+9, error-9,
                                 bitField);
                    WakeupNextPacket();
                    break;
                case 5:
                    len = __last_piece_size?__piece_count+1:__piece_count;
                    len = (len+7) / 8;
                    if (error != len+1) {
                        nextState = 9;
                    }
                    local_interesting = 1;
                    memcpy(bitField, buffer + 1, len);
                    WakeupNextPacket();
                    break;
                case 4:
                    local_interesting = 1;
                    memcpy(&index, buffer + 1, 4);
                    index = ntohl(index);
                    bitfield_set(bitField, index);
                    bitfield_set(haveField, index);
                    WakeupNextPacket();
                    break;
                case 8:
                    memcpy(&request, buffer + 1, sizeof(request));
                    cancel_request(&request);
                    break;
                default:
                    printf("unkown cmd: %x\n", buffer[0] & 0xFF);
                    break;
                }
                nextState = 5;
            }
            break;
        default:
            reassign_segment(bitField);
            dumpPeerInfo();
            peerSocket.Close();
            goodPeer = 0;
            state = 0;
            return 0;
            break;
        }
    }
    return error;
}

#define INTERESTED_BONUS 2.0
#define CHOKED_PENALTY 0.75
#define NEWPEER_LEVEL 4000
#define OLDPEER_LEVEL 0

int PeerProcess::remote_rate_timer()
{
    int total;
    total = remote_total_time;
    if (remote_send_time != 0) {
        total += time(NULL)-remote_send_time;
    }
    if (total < 1) {
        total = 1;
    }
    return total;
}

void PeerProcess::start_remote_timer()
{
    if (remote_send_time == 0) {
        remote_send_time = time(NULL);
    }
}

void PeerProcess::stop_remote_timer()
{
    if (remote_send_time != 0) {
        remote_total_time += time(NULL)-local_send_time;
        remote_send_time = 0;
    }
}

void PeerProcess::start_local_timer()
{
    if (local_send_time == 0) {
        local_send_time = time(NULL);
    }
}

void PeerProcess::stop_local_timer()
{
    if (local_send_time != 0) {
        local_total_time += time(NULL)-local_send_time;
        timeUsed[timeRing] += time(NULL)-local_send_time;
        local_send_time = 0;
    }
}

int PeerProcess::local_rate_timer()
{
    int total;
    total = local_total_time;
    if (local_send_time != 0) {
        total += time(NULL)-local_send_time;
    }
    if (total < 1) {
        total = 1;
    }
    return total;

}

int PeerProcess::getRate()
{
    int i;
    int totalByte = 0;
    int totalTime = 0;
    for (i=0; i<100; i++) {
        totalByte += readBytes[i];
        totalTime += timeUsed[i];
    }
    if (local_send_time != 0) {
        totalTime += time(NULL)-local_send_time;
    }
    if (totalTime < 1) {
        totalTime = 1;
    }
    return totalByte/totalTime;
}

int PeerProcess::getSpeed()
{
    int newpeer;
    float atime, arate;
    atime = (float)remote_rate_timer();
    newpeer = local_rate_timer();
#if 0
    if (newpeer < 30 && local_interesting==1) {
        arate = (float)NEWPEER_LEVEL;
    } else if (atime<30) {
        arate = (float)OLDPEER_LEVEL;
    } else {
#endif
        arate = getRate();
#if 0
    }
#endif
    if (local_interesting==1) {
        arate *= (float)INTERESTED_BONUS;
    }
    if (local_interesting==0 || remote_chocked==1) {
        arate *= (float)CHOKED_PENALTY;
    }
    return arate;
}
