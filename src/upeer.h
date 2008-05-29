class PeerProcess;

class PeerSocket:public fSocket, public fHandle
{
public:
	PeerSocket();
	int Abort();
    int RunActive();
    int getInChanelId();
    int setOutChanelId(int id);
    int waitPacket(fHandle * handle);
    int Attach(int fild, sockaddr_in * addr, int addrlen, int wait);
    int RecvPacket(char buf[], int len);
    int SendPacket(char buf[], int len);
	int display();
	unsigned long address;
	unsigned short port;
	int fildes;
	int need_active(){
		return (state==0);
	}

	int attach(fHandle *handle){
		if (caller == NULL || caller==handle){
		   	caller = handle;
			return 0;
		}
		return -1;
	}

	int detach();

	int dosomething();
    char peer_ident[20];


private:
	time_t hand_shake_time;
	int set_hand_shake(const char *buffer, int len);
	PeerProcess *peer;
	fHandle *caller;
	int state;
    int SendAll(char buf[], int len);
    int loff;
    int offsetIn;
    int lengthIn;
    char packetIn[81920];
    int tOffset;
    int tLength;
    char tBuffer[81920];
    fHandle * writeHandle;
};
