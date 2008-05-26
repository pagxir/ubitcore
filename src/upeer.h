class PeerSocket:public fSocket, public fHandle
{
public:
    int RunActive();
    int getInChanelId();
    int setOutChanelId(int id);
    int waitPacket(fHandle * handle);
    int Attach(int fild, sockaddr_in * addr, int addrlen, int wait);
    int RecvPacket(char buf[], int len);
    int SendPacket(char buf[], int len);
	int display();
	void *owner;
	void *writer;
private:
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
