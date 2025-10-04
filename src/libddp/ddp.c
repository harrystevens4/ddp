#include "ddp.h"
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>

int ddp_broadcast_socket(){
	//====== getaddrinfo a broadcast address ======
	struct addrinfo hints, *address_info;
	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_family = AF_UNSPEC;
	//address
	int result = getaddrinfo("255.255.255.255",DDP_PORT_STRING,&hints,&address_info);
	if (result < 0){
		fprintf(stderr,"getaddrinfo: %s\n",gai_strerror(result));
		return -1;
	}
	//socket
	int sockfd = socket(address_info->ai_family,address_info->ai_socktype,0);
	if (sockfd < 0){
		perror("socket");
		return -1;
	}
	//enable broadcast
	int optval = 1;
	result = setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,&optval,sizeof(optval));
	if (result < 0){
		perror("setsockopt");
		return -1;
	}
	//bind
	result = bind(sockfd,address_info->ai_addr,address_info->ai_addrlen);
	if (result < 0){
		perror("bind");
		return -1;
	}
	return sockfd;
}
int ddp_broadcast(int sockfd, int request){
	//pack a broadcast address
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_addr = 0xffffffff
	};
	addr.sin_port = 
	socklen_t addrlen = sizeof(addr);
	sendto();
}
