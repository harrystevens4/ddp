#include "ddp.h"
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>

int ddp_new_socket(int timeout_ms){
	//====== getaddrinfo a broadcast address ======
	struct addrinfo hints, *address_info;
	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_family = AF_INET;
	//====== address ======
	int result = getaddrinfo(NULL,DDP_PORT_STRING,&hints,&address_info);
	if (result < 0){
		fprintf(stderr,"getaddrinfo: %s\n",gai_strerror(result));
		return -1;
	}
	//====== socket ======
	int sockfd = socket(address_info->ai_family,address_info->ai_socktype,0);
	freeaddrinfo(address_info);
	if (sockfd < 0){
		perror("socket");
		return -1;
	}
	//====== enable broadcast ======
	int optval = 1;
	result = setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,&optval,sizeof(optval));
	if (result < 0){
		perror("setsockopt");
		return -1;
	}
	//====== set timeout ======
	struct timeval timeout_tv = {
		.tv_sec = timeout_ms/1000,
		.tv_usec = (timeout_ms*1000) % 1000000,
	};
	if (setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&timeout_tv,sizeof(timeout_tv)) < 0){
		perror("setsockopt");
		return -1;
	}
	if (setsockopt(sockfd,SOL_SOCKET,SO_SNDTIMEO,&timeout_tv,sizeof(timeout_tv)) < 0){
		perror("setsockopt");
		return -1;
	}
	return sockfd;
}
int ddp_broadcast(int sockfd, int request, void *data, size_t len){
	//====== pack a broadcast address ======
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_addr = {0xffffffff},
		.sin_port = htons(DDP_PORT),
	};
	socklen_t addrlen = sizeof(addr);
	//====== create header ======
	struct ddp_header header = {
		.type = request,
		.body_size = len,
	};
	gethostname(header.hostname,255);
	//====== create an iovec ======
	struct iovec packet_iovec[2] = {
		{&header,sizeof(header)},
		{data,len},
	};
	//====== prep a msghdr ======
	struct msghdr message = {
		.msg_name = &addr,
		.msg_namelen = addrlen,
		.msg_iov = packet_iovec,
		.msg_iovlen = 2,
	};
	int result = sendmsg(sockfd,&message,0);
	if (result < 0){
		perror("sendmsg");
		return -1;
	}
	return 0;
}
int ddp_respond(int sockfd, int response, struct iovec *data_vec, size_t data_vec_len, struct sockaddr *addr, socklen_t addrlen){
	//====== fill in a ddp header ======
	size_t body_size = 0;
	//calc the total body size
	for (size_t i = 0; i < data_vec_len; i++) body_size += data_vec[i].iov_len;
	struct ddp_header response_header = {
		.type = response,
		.body_size = body_size,
	};
	if (gethostname(response_header.hostname,sizeof(response_header.hostname)) < 0){
		perror("gethostname");
		return -1;
	}
	//new iovec to include header
	struct iovec *response_vec = calloc(data_vec_len+1,sizeof(struct iovec));
	response_vec->iov_base = &response_header;
	response_vec->iov_len = sizeof(response_header);
	memcpy(response_vec+1,data_vec,data_vec_len*sizeof(struct iovec));
	//====== semd the message ======
	struct msghdr message = {
		.msg_name = addr,
		.msg_namelen = addrlen,
		.msg_iov = response_vec,
		.msg_iovlen = data_vec_len+1,
	};
	long int count = sendmsg(sockfd,&message,0);
	//printf("sent %ld bytes\n",count);
	free(response_vec);
	if (count < 0){
		perror("sendmsg");
		return -1;
	}
	return 0;
}
long ddp_receive_response(int sockfd, char **return_buffer, struct sockaddr *addr, socklen_t *addrlen){
	//====== receive untill we find a response ======
	for (;;){
		//memset my goat
		struct ddp_header header;
		memset(&header,0,sizeof(header));
		//====== we need the header to get the body size ======
		if (recv(sockfd,&header,sizeof(header),MSG_PEEK) < 0){
			if (errno == EAGAIN || errno == EWOULDBLOCK) return -EAGAIN; //no data available
			perror("recv");
			return -1;
		}
		size_t packet_size = header.body_size + sizeof(header);
		//chat should i use alloca?
		char *buffer = malloc(packet_size);
		//====== receive the packet fr this time ======
		long int size = recvfrom(sockfd,buffer,packet_size,0,addr,addrlen);
		if (size < packet_size){
			perror("recvfrom");
			return -1;
		}
		if (header.type > 0){
			//request
			free(buffer);
		}else {
			//response
			//printf("received %ld bytes\n",size);
			*return_buffer = buffer;
			return packet_size;
		}
	}
}
