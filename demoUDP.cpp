#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>


#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


#define MAZEPORT 40037
#define MAZEGROUP 0xe0010101

typedef struct sockaddr_in Sockaddr;

typedef struct {
        char data[24];
} Packet;


static Sockaddr groupAddr;
int mySocket;
socklen_t fromLen = sizeof(struct sockaddr_in);

void netInit();

void receivePacket(){
	fd_set        fdmask;
        FD_ZERO(&fdmask);
        FD_SET(mySocket, &fdmask);

	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;


	int rtn;
	Packet received;
	while(true){
		rtn = select(32, &fdmask, NULL, NULL, &tv);
		if(rtn){
			recvfrom(mySocket, (char*)&received, sizeof(Packet), 0, (struct sockaddr *) &groupAddr, &fromLen);
			printf("received from: %s\n", received.data);
		}else{
			printf("timeout\n");
			return;
		}
	}
}

int main(){
	netInit();

	srand (time(NULL));
	char data[128];
	sprintf( data, "%d\n", rand()%100000);

	printf("my name is: %s\n", data);

	Packet p;
	memset(&p, 0, sizeof(p));
	memcpy(p.data, data, strlen(data));

	while(true){
		sleep(1);
		sendto(mySocket, &p, sizeof(p), 0, (struct sockaddr *) &groupAddr, sizeof(Sockaddr));
		sendto(mySocket, &p, sizeof(p), 0, (struct sockaddr *) &groupAddr, sizeof(Sockaddr));
		receivePacket();
	}
}
void
netInit()
{
	Sockaddr		nullAddr;
	int				reuse;
	u_char          ttl;
	struct ip_mreq  mreq;

	mySocket = socket(AF_INET, SOCK_DGRAM, 0);

	reuse = 1;
	if (setsockopt(mySocket, SOL_SOCKET, SO_REUSEADDR, &reuse,
		   sizeof(reuse)) < 0) {
		printf("setsockopt failed (SO_REUSEADDR)\n");
	}

	nullAddr.sin_family = AF_INET;
	nullAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	nullAddr.sin_port = htons(MAZEPORT);
	if (bind(mySocket, (struct sockaddr *)&nullAddr,
		 sizeof(nullAddr)) < 0)
	  printf("netInit binding");

	ttl = 1;
	if (setsockopt(mySocket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
		   sizeof(ttl)) < 0) {
		printf("setsockopt failed (IP_MULTICAST_TTL)");
	}

	/* join the multicast group */
	mreq.imr_multiaddr.s_addr = htonl(MAZEGROUP);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(mySocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)
		   &mreq, sizeof(mreq)) < 0) {
		printf("setsockopt failed (IP_ADD_MEMBERSHIP)");
	}

	memcpy(&groupAddr, &nullAddr, sizeof(Sockaddr));
	groupAddr.sin_addr.s_addr = htonl(MAZEGROUP);
}
