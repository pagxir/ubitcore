class fHandle
{
public:
    fHandle();
    int Wakeup();
    virtual int Abort();
    virtual int RunActive();
    static int getNextReady(fHandle ** handle);
private:
    int flags;
};
int WaitTime(int nt);
int ClearCallAgain();
int WouldCallAgain(fHandle * handle);
int SetCallAgainHandle(int (*)(fHandle *, int, void *), int, void *);
