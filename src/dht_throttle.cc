#include <stdio.h>
#include <fhandle.h>
#include <stdlib.h>
#include <assert.h>
#include <dht_throttle.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <winsock.h>
#include <fsocket.h>
#include <queue>
#include <btdump.h>

struct dht_node_t{
   u_long dn_address;
   u_short dn_port;
};

static std::queue<dht_node_t> __dht_node_queue;
int dht_add_node(const char *address, int port)
{
    dht_node_t dn;
    dn.dn_address = inet_addr(address);
    dn.dn_port = port;
    __dht_node_queue.push(dn);
    printf("%s:%d\n", address, port);
    return 0;
}

class udp_socket_t: fSocket
{
   public:
      udp_socket_t();
      int sendto(const char *buf, int len, int flag,
          const sockaddr *name, socklen_t namelen);
      int recvfrom(char *buf, int len, int flag, 
          sockaddr *name, socklen_t *namelen);
};

udp_socket_t::udp_socket_t(): fSocket(SOCK_DGRAM)
{
}

int udp_socket_t::sendto(const char *buf, int len, int flag,
         const sockaddr *name, socklen_t namelen)
{
   ioctlsocket(file, 0, NULL);
   int count = ::sendto(file, buf, len, flag, name, namelen);
   assert(count == len);
   return count;      
}

int udp_socket_t::recvfrom(char *buf, int len, int flag, 
          sockaddr *name, socklen_t *namelen)
{
   int count = ::recvfrom(file, buf, len, flag, name, namelen);
   printf("rfrom: %d\n", file);
   return SetWaitRead(this, count);
}

class dht_throttle: public fHandle 
{
public:
    dht_throttle();
    int Abort();
    int RunActive();
private:
    udp_socket_t udp_socket;
};

dht_throttle::dht_throttle()
{
}

static int dump_hex_get_peers(const char *buf, const char *buf_end)
{
   printf("get peer packet\n");
   //const char *
   printf("\n");
   return 0;
}

static int dump_peer_info(const char *chbase, int count)
{
   in_addr address;
   u_short port;
   for(int i=0; i<count; i+=6){
      if (i+6<=count){
        memcpy(&address, chbase+i, 4);
        memcpy(&port, chbase+i+4, 2);
        port = htons(port);
        printf("inet4: %s:%d\n", inet_ntoa(address), port);
      }
   }
   return 0;
}

typedef dht_node_t dht_peer_t;
static std::queue<dht_peer_t> __dht_peer_queue;
int dht_get_peers(const char buf[], int count, 
    const sockaddr_in *name, socklen_t namelen)
{
   const char *buf_end = buf+count;
   const char *t = btload_dict(buf, buf_end, "1:t");
   if (t >= buf_end){
      return -1;
   }
   const char *y = btload_dict(buf, buf_end, "1:y");
   if (y >= buf_end){
      return -1;
   }
   const char *resp = btload_dict(buf, buf_end, "1:r");
   if (resp >= buf_end){
      return -1;
   }
   if (strncmp(y, "1:r", 3) != 0){
      return -1;
   }
   if (strncmp(t, "1:0", 3) == 0){
      dht_peer_t dp;
      dp.dn_address = ((sockaddr_in*)name)->sin_addr.s_addr;
      dp.dn_port = ntohs(((sockaddr_in*)name)->sin_port);
      __dht_peer_queue.push(dp);
      return 0;
   }
   if (strncmp(t, "1:2", 3) == 0){
      const char *id = btload_dict(resp, buf_end, "2:id");
      const char *tokern = btload_dict(resp, buf_end, "5:token");
      const char *nodes = btload_dict(resp, buf_end, "5:nodes");
      const char *values = btload_dict(resp, buf_end, "6:values");
      if (values < buf_end){
          printf("peer return from DHT!\n");
          int count = 0;
          int slen = 0;
          const char *chbase = btload_str(values, buf_end, &slen);
          dump_peer_info(chbase, count);
      }
      if (nodes < buf_end){
          printf("nodes return from DHT!\n");
          int count = 0;
          int slen = 0;
          const char *chbase = btload_str(nodes, buf_end, &slen);
          dump_peer_info(chbase, count);
      }
   }
   return 0;
}

int dht_throttle::Abort()
{
   printf("DHT Abort!\n");
   return 0;
}
const char *get_bin_info_hash();
int build_get_peer(char *buf, int size)
{
   const char temple[] = 
      "d1:ad2:id20:abcdefghij01234567899:info_hash20:mnopqrstuvwxyz123456e1:q9:get_peers1:t1:21:y1:qe";
   memcpy(buf, temple, strlen(temple));
   memcpy(buf+46, get_bin_info_hash(), 20);
   return strlen(temple);
}

int dht_throttle::RunActive()
{
   sockaddr_in sa;
   dht_node_t  dn;
   dht_peer_t  dp;
   char dht_buf[1024] =
      "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t1:01:y1:qe";
   int dht_size = strlen(dht_buf);

   while(!__dht_node_queue.empty()){
     dn = __dht_node_queue.front();
      __dht_node_queue.pop();
     sa.sin_family = AF_INET;
     sa.sin_port = htons(dn.dn_port);
     sa.sin_addr.s_addr = dn.dn_address;
     printf("ping %s %d!\n", inet_ntoa(sa.sin_addr), ntohs(sa.sin_port));
     udp_socket.sendto(dht_buf, dht_size, 0, (sockaddr*)&sa, sizeof(sa));
   }
   int count = 0;
   socklen_t salen = sizeof(sa);
   while ((count=udp_socket.recvfrom(dht_buf,
	 sizeof(dht_buf), 0, (sockaddr*)&sa, &salen)) != -1){
       dht_get_peers(dht_buf, count, &sa, salen);
       printf("DHT packet recv!\n");
   }
   while (!__dht_peer_queue.empty()){
     dp = __dht_peer_queue.front();
     __dht_peer_queue.pop();
     sa.sin_family = AF_INET;
     sa.sin_port = htons(dp.dn_port);
     sa.sin_addr.s_addr = dp.dn_address;
     dht_size = build_get_peer(dht_buf, sizeof(dht_buf));
     udp_socket.sendto(dht_buf, dht_size, 0, (sockaddr*)&sa, sizeof(sa));
   }
   return count;
}

static dht_throttle __dht_throttle;
int dht_throttle_start()
{
    __dht_throttle.Wakeup();
    return 0;
}
