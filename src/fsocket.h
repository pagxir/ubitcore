class fSocket
{
public:
    int rCount;
    fSocket();
    fSocket(int type);
    int isPassive();
    int Connected();
    int setBlockopt(int block);
    int Connect(sockaddr * addr, int addrsize);
    int getSockaddr(sockaddr * addr, int *addrsize);
    int Send(char *buf, int len, int flag = 0);
    int Recv(char *buf, int len, int flag = 0);
    int Close();
    int WaitRead(fHandle * handle);
    int WaitWrite(fHandle * handle);
    int WaitExcept(fHandle * handle);
	virtual int display(){ return 0; }
    static int Select(int timeout);
protected:
    int sock_type;
protected:
    int file;
	const char *msg;
	int sock_close;
    int connected;
    int passiveMode;
    sockaddr_in address;
    int Suppend();
    int SetExcept(int code);
    int SetPoll(fd_set * readfds, fd_set * writefds, fd_set * exceptfds);
    int GetPoll(fd_set * readfds, fd_set * writefds, fd_set * exceptfds);
    char logBuffer[8192];
private:
    int flags;
    unsigned long blkOptval;
    fHandle * readHandle;
    fHandle * writeHandle;
    fHandle * exceptHandle;
};
int SetWaitRead(void *file, int code);
int SetWaitWrite(void *file, int code);
int SetWaitConnect(void *file, int code);
