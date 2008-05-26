class btlink_process: public fHandle  
{
public:
  btlink_process();
  int RunActive();
  int Abort();

private:
  PeerProcess *peer;
};
