#define CACHE 1024

struct ChanelContext;
class PeerSession {
public:
    PeerSession(ChanelContext *);
    int Wakeup();
    int ReInit();
    int saveIndex(int index);
    int setWait(fHandle * handle);
    int writePacket(PeerSocket * peerSocket);
    int savePacket(char buf[], int len);
    ChanelContext * getContext();
protected:
    int getRequestBlock();
    int updateBlock(int index);
    int saveHaveBlock(char *buf, int len);
    int saveBitfield(char *buf, int len);
    int saveDataPiece(char *buf, int len);
    int saveRequest(char *buf, int len);
private:
    int bitOffset;
    int interested;
    int bitField[CACHE];
    int outRequest[64];
    int idxHave;
    std::queue < int >inRequest;
    fHandle * waitHandle;
    ChanelContext * chanelContext;
};
struct ChanelContext {
    int QosBlock;
    int offsetBlock;
    int tickBlock[CACHE];
    int InRequesting[CACHE];
    int lastHaveCount;
    int lastHaveIndex[CACHE];
    int blockLength[CACHE];
    char *blockList[CACHE];
    std::queue < PeerSession * >waitingQueue;
    int updateTickBlock(int index);
    int getBitfield(char buf[], int len);
    int saveDataBlock(int index, char buf[], int len);
    int SetPriority(int block);
    int WakeupAll(int index);
    int setInRequesting(int index);
    int sendDataBlock(PeerSocket * peerSocket, int index);
};
extern ChanelContext __static_VideoChanel;
extern ChanelContext __static_AudioChanel;
extern int SaveVideoPiece(int index, char buf[], int len);
