#include <stdio.h>
#include <assert.h>
#include <winsock.h>

int main(int argc, char * argv[])
{
	int error;
	int sockfd;
	WSADATA data;
	struct sockaddr_in so_addr;
	WSAStartup(0x101, &data);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	assert(sockfd != -1);

	so_addr.sin_family = AF_INET;
	so_addr.sin_port   = htons(8080);
	so_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	sendto(sockfd, "HELLO", 5, 0,
			(struct sockaddr *)&so_addr, sizeof(so_addr));

	closesocket(sockfd);

	WSACleanup();
	return 0;
}

