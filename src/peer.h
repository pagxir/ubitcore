class PeerProcess;
struct request_t {
    int q_piece;
    int q_start;
    int q_length;
};
class PeerWriteProcess:public fHandle  {
public:
    PeerWriteProcess(PeerProcess * handle);
    int RunActive();
private:
    PeerProcess * peerProcess;
};

class PeerProcess:public fHandle  {
public:
	int fildes;
    char peer_ident[20];
	unsigned long address;
	unsigned short port;

	int attach(fHandle *handle){
		if (caller == NULL || caller==handle){
		   	caller = handle;
			return 0;
		}
		return -1;
	}

	int detach(){
		caller = NULL;
	}

    PeerProcess();
    int writeNextPacket();
    int RunActive();
    int Chock();
    int Unchock();
    int IsActive();
	int need_active(){
		return state==9;
	}
    int IsInterest() {
        return local_interesting;
    };
    int IsRemoteInterested() {
        return remote_insterested;
    }
    int IsRemoteChock() {
        return remote_chocked;
    }
    int NextRing() {
        timeRing++;
        timeRing %= 100;
        timeUsed[timeRing] = 0;
        readBytes[timeRing] = 0;
        connectTime++;
    }
    int QuickChock(){
	    local_next_chocked = 1;
	    local_chocking = 1;
	    while (!mRequests.empty()){
		    mRequests.pop();
	    }
	    return 0;
    }
    int getConnectTime() {
        return connectTime;
    }
    int getSpeed();
    int Abort();
private:
	fHandle *caller;
    int connectTime;
    int state;
    char *bitField;
    char *haveField;
    PeerWriteProcess writeProcess;
    sockaddr_in peerName;
    PeerSocket peerSocket;
private:
    std::queue < request_t > mRequests;
private:
    int queued;
    int remote_chocked;
    int remote_insterested;

private:
    int local_chocked;
    int local_next_chocked;
    int local_chocking;
    int local_interested;
    int local_interesting;

    static int totalReceive;
private:
    int hand_shake_time;
    int saveAddress(char *buffer, int length);
    int dumpHandShake(char *buf, int len);
    int dumpPeerInfo();
private:
    int remote_total_time;
    int remote_send_time;
    int remote_rate_timer();
private:
    int local_total_time;
    int local_send_time;
    int local_rate_timer();
private:
    void start_local_timer();
    void stop_local_timer();
    void start_remote_timer();
    void stop_remote_timer();
    int getRate();
    void cancel_request(request_t *r);
private:
    int goodPeer;
    int lastRead;
    int readCount;

    int timeRing;
    int readBytes[256];
    int timeUsed[256];
};


