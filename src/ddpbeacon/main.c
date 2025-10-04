#include <stdio.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include "../libddp/ddp.h"

int respond_empty(int socket, struct sockaddr *addr, socklen_t addrlen);
int respond_addresses(int sockfd, struct sockaddr *addr, socklen_t addrlen);

int main(){
	//====== getaddrinfo a broadcast address ======
	struct addrinfo hints, *address_info;
	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_family = AF_UNSPEC;
	//address
	int result = getaddrinfo(NULL,DDP_PORT_STRING,&hints,&address_info);
	if (result < 0){
		fprintf(stderr,"getaddrinfo: %s\n",gai_strerror(result));
		return 1;
	}
	//socket
	int sockfd = socket(address_info->ai_family,address_info->ai_socktype,0);
	if (sockfd < 0){
		perror("socket");
		return 1;
	}
	//bind 
	result = bind(sockfd,address_info->ai_addr,address_info->ai_addrlen);
	if (result < 0){
		perror("bind");
		return 1;
	}
	//cleanup
	freeaddrinfo(address_info);
	//====== mainloop =======
	for (;;){
		//====== read message header ======
		struct ddp_header message_header;
		memset(&message_header,0,sizeof(message_header));
		struct sockaddr sender_addr;
		socklen_t sender_addrlen = sizeof(sender_addr);
		recvfrom(sockfd,&message_header,sizeof(message_header),0,&sender_addr,&sender_addrlen);
		//====== process requests and discard responses ======
		//a type less than 0 indicates a response
		if (message_header.type < 0) continue;
		switch (message_header.type){
		case DDP_REQUEST_EMPTY:
			respond_empty(sockfd,&sender_addr,sender_addrlen);
			break;
		case DDP_REQUEST_DISCOVER_ADDRESSES:
			respond_addresses(sockfd,&sender_addr,sender_addrlen);
			break;
		}
	}
	//shutdown
	close(sockfd);
}
char *get_host_name(){
	static char buffer[256] = {0};
	gethostname(buffer,255);
	return buffer;
}
int respond_empty(int socket, struct sockaddr *addr, socklen_t addrlen){
	//====== simply send an empty response ======
	struct ddp_header response_header = {
		.type = DDP_RESPONSE_EMPTY,
		.body_size = 0,
	};
	strcpy(response_header.hostname,get_host_name());
	int result = write(socket,&response_header,sizeof(response_header));
	if (result < 0){
		perror("write");
		return -1;
	}
	result = sendto(socket,&response_header,sizeof(response_header),0,addr,addrlen);
	if (result < 0){
		perror("sendto");
		return -1;
	}
	return 0;
}
int respond_addresses(int sockfd, struct sockaddr *addr, socklen_t addrlen){
	//====== get all network interface addresses ======
	struct ifaddr *local_addrs;
	if (getifaddrs(&local_addrs) < 0){
		perror("getifaddrs");
		return -1;
	}
	//====== ignore any loopback addresses ======
	struct ddp_header response_header = {
		.type = DDP_RESPONSE_DISCOVER_ADDRESSES,
	};
	if (gethostname(&response_header.hostname,sizeof(response_header.hostname)) < 0){
		perror("gethostname");
		freeifaddrs(local_addrs);
		return -1;
	}
	//prepare iovec for addresses
	struct iovec *addr_vec = malloc(sizeof(struct iovec));
	addr_vec[0].iov_base = &response_header; //wait writev kinda goes hard
	addr_vec[0].iov_len = sizeof(response_header);
	size_t addr_vec_count = 1;
	for (struct ifaddr *next = local_addrs; next != NULL; next = next->ifa_next){
		if (next->ifa_flags & IFF_LOOPBACK) continue;
		addr_vec_count++;
		addr_vec = realloc(addr_vec,sizeof(struct iovec)*addr_vec_count);
		addr_vec[addr_vec_count-1].iov_base = &next->ifa_addr;
		addr_vec[addr_vec_count-1].iov_len = sizeof(struct sockaddr);
	}
	//====== set the destination address so we can writev ======
	if (connect(sockfd,addr,addrlen) < 0){
		perror("connect");
		freeifaddrs(local_addrs);
		free(addr_vec);
		return -1;
	}
	//====== send the data ======
	response_header.body_size = sizeof(struct sockaddr) * (addr_vec_count-1);
	long int count = writev(sockfd,addr_vec,addr_vec_count);
	if (count < 0){
		perror("writev");
		freeifaddrs(local_addrs);
		free(addr_vec);
		return -1;
	}
	freeifaddrs(local_addrs);
	//====== un connect the socket ======
	struct sockaddr reset_addr = {
		.sa_family = AF_UNSPEC,
	};
	if (connect(sockfd,&reset_addr,sizeof(reset_addr))){
		perror("connect");
		return -1;
	}
}
