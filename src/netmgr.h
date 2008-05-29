class PeerProcess;

class NetMgr
{
public:
    NetMgr();
    int savePeer(unsigned long inaddr, unsigned short port);
    int SetActivePeer(int fild, sockaddr_in * addr, int addrlen);
	static PeerSocket *GetNextSocket();
private:
    int fild;
    sockaddr_in peername;
};
int get_comunicate_count();
PeerProcess *attach_peer(PeerSocket *psocket, const char ident[20]);
int loadPeers(char *buf, int len);
int SetWaitNewClient(int code);
int getSelfAddress(char *buf, int len);
int startSession(sockaddr_in * peerName, int addrlen);
int SetActivePeer(int fild, sockaddr_in * addr, int addrlen);
int LoadTorrentPeer(char *buffer, int len);

