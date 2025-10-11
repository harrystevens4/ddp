#include "ddp.h"
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
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
int ddp_sendto(int sockfd, int request, void *data, size_t len, struct sockaddr *addr, socklen_t addrlen){
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
		.msg_name = addr,
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
int ddp_broadcast(int sockfd, int request, void *data, size_t len){
	//====== pack a broadcast address ======
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_addr = {0xffffffff},
		.sin_port = htons(DDP_PORT),
	};
	socklen_t addrlen = sizeof(addr);
	//====== send it off ======
	return ddp_sendto(sockfd,request,data,len,(struct sockaddr *)&addr,addrlen);
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
			free(buffer);
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

const char *sockaddr_to_string(struct sockaddr *addr){
	static char buffer[1024];
	memset(buffer,0,sizeof(buffer));
	if (addr->sa_family == AF_INET){
		struct sockaddr_in *in_addr = (struct sockaddr_in *)addr;
		return inet_ntop(AF_INET,&in_addr->sin_addr,buffer,sizeof(buffer));
	}
	if (addr->sa_family == AF_INET6){
		struct sockaddr_in6 *in6_addr = (struct sockaddr_in6 *)addr;
		return inet_ntop(AF_INET6,&in6_addr->sin6_addr,buffer,sizeof(buffer));
	}
	return NULL;
}
long int ddp_query(int timeout_ms, int request, ddp_response_t **responses){
	//====== prep a response ======
	int response_count = 0;
	ddp_response_t *current_response = NULL;
	*responses = NULL;
	//====== start a timer ======
	TIMER timer = timer_new(timeout_ms);
	//====== send a discovery request ======
	int sockfd = ddp_new_socket(timeout_ms);
	if (sockfd < 0) return -1;
	int result = ddp_broadcast(sockfd,request,NULL,0);
	if (result < 0){
		return -1;
		close(sockfd);
	}
	//====== await responses ======
	for (;;){
		ddp_response_t response = {
			.addrlen = sizeof(struct sockaddr),
		};
		//====== has timeout expired? ======
		if (timer_expired(&timer)) break;
		char *packet = NULL;
		//====== get a response ======
		long packet_size = ddp_receive_response(
			sockfd,&packet,&response.addr,&response.addrlen
		);
		if (packet_size < 0) break;
		memcpy(&response.header,packet,sizeof(struct ddp_header));
		size_t data_len = packet_size-sizeof(struct ddp_header);
		response.data = malloc(data_len);
		response.data_len = data_len;
		memcpy(response.data,packet+sizeof(struct ddp_header),data_len);
		free(packet);
		response_count++;
		//====== add response to linked list of responses ======
		if (current_response == NULL){
			current_response = malloc(sizeof(ddp_response_t));
			*responses = current_response;
			memcpy(current_response,&response,sizeof(ddp_response_t));
		}else {
			current_response->next = malloc(sizeof(ddp_response_t));
			memcpy(current_response->next,&response,sizeof(ddp_response_t));
			current_response = current_response->next;
		}
	}
	close(sockfd);
	return response_count;
}
void ddp_free_responses(ddp_response_t *responses){
	for (ddp_response_t *current = responses; current != NULL;){
		free(current->data); 
		//carefull not to use after free
		ddp_response_t *next = current->next;
		free(current);
		current = next;
	}
}
TIMER timer_new(int duration_ms){
	struct timespec end_time = {0};
	int result = clock_gettime(CLOCK_BOOTTIME,&end_time);
	if (result != 0){
		perror("clock_gettime");
	}
	end_time.tv_sec += duration_ms/1000;
	end_time.tv_nsec += (duration_ms%1000)*1000000;
	return end_time;
}
int timer_expired(TIMER *timer){
		struct timespec time_now;
		int result = clock_gettime(CLOCK_BOOTTIME,&time_now);
		if (result != 0){
			perror("clock_gettime");
			return -1;
		}
		if (time_now.tv_sec > timer->tv_sec || (time_now.tv_nsec >= timer->tv_nsec && time_now.tv_sec == timer->tv_sec)) return 1;
		return 0;
}
