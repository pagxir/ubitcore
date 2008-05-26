class PeerProcess;

class NetMgr
{
public:
    NetMgr();
    int savePeer(unsigned long inaddr, unsigned short port);
    int SetActivePeer(int fild, sockaddr_in * addr, int addrlen);
    static int NextClient(PeerSocket * peerSocket);
	static PeerProcess *GetNextPeer();
private:
    int fild;
    sockaddr_in peername;
};
int get_comunicate_count();
int try_add_peer_id(PeerProcess *peer);
int loadPeers(char *buf, int len);
int SetWaitNewClient(int code);
int getSelfAddress(char *buf, int len);
int startSession(sockaddr_in * peerName, int addrlen);
int SetActivePeer(int fild, sockaddr_in * addr, int addrlen);
int LoadTorrentPeer(char *buffer, int len);

