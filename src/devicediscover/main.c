#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "../libddp/ddp.h"


int discover_addresses();
int ping(int timeout_ms, char *address);

void print_help(){
	printf("devicediscover <command> [options]\n");
	printf("commands:\n");
	printf("	addresses      : print the hostnames and ip addresses of all broadcasting devices\n");
	printf("	help           : show this text\n");
	printf("	ping <address> : ping a host to see if they respond\n");
}

//thank you http://www.cse.yorku.ca/~oz/hash.html
unsigned short hash(char *str){
	unsigned short hash = 5381;
	for (int c = *str; c != 0; c = *str++){
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}
	return hash;
}

int main(int argc, char **argv){
	//printf("%d\n",hash("addresses"));
	int timeout = 200;
	//====== look at arguments ======
	struct option long_options[] = {
		{"help",no_argument,0,'h'},
		{0,0,0,0}
	};
	int options_index;
	for (;;){
		int result = getopt_long(argc,argv,"h",long_options,&options_index);
		if (result == -1) break;

		switch (result){
		case 'h':
			print_help();
			return 0;
		}
	}
	if (optind >= argc){
		fprintf(stderr,"Expected argument of <command>\n");
		return 1;
	}
	//====== process subcommands ======
	switch (hash(argv[optind])){ //hash produces a magic number that i have precalculated
	case 47094:
		print_help();
		break;
	case 23108:
		discover_addresses(timeout);
		break;
	case 61507:
		if (optind+1 >= argc){
			fprintf(stderr,"Expected address or hostname\n");
			return 1;
		}
		ping(timeout,argv[optind+1]);
		break;
	default:
		fprintf(stderr,"Unknown command \"%s\"\n",argv[optind]);
		return 1;
	}
	return 0;
}

int ping(int timeout_ms, char *address){
	//====== new socket ======
	int sockfd = ddp_new_socket(timeout_ms);
	if (sockfd < 0) return -1;
	//====== find address of host ======
	struct addrinfo *addrs, hints = {
		.ai_socktype = SOCK_DGRAM,
		.ai_family = AF_UNSPEC,
	};
	int result = getaddrinfo(address,DDP_PORT_STRING,&hints,&addrs);
	if (result < 0){
		fprintf(stderr,"getaddrinfo: %s\n",gai_strerror(result));
		return -1;
	}
	//====== sent them an empty request ======
	result = ddp_sendto(sockfd,DDP_REQUEST_EMPTY,NULL,0,addrs->ai_addr,addrs->ai_addrlen);
	freeaddrinfo(addrs);
	if (result < 0) return -1;
	//start timer
	TIMER timer = timer_new(timeout_ms);
	//====== await a response ======
	for (;;){
		if (timer_expired(&timer)) break;
		char *packet;
		long packet_size = ddp_receive_response(sockfd,&packet,NULL,0);
		if (packet_size < 0){
			if (errno == EAGAIN) continue;
			else break;
		}
		//====== is empty packet ======
		struct ddp_header header;
		memcpy(&header,packet,sizeof(header));
		free(packet);
		if (header.type == DDP_RESPONSE_EMPTY){
			printf("Response received.\n");
			return 0;
		}
	}
	printf("No response.\n");
	return -1;
}

int discover_hostnames(int timeout){
	//====== send a discovery request ======
	ddp_response_t *responses;
	int count = ddp_query(timeout,DDP_REQUEST_DISCOVER_ADDRESSES,&responses);
	if (count < 0) return -1;
	for (ddp_response_t *response = responses; response != NULL; response = response->next){
		printf("%s\n",response->header.hostname);
	}
	ddp_free_responses(responses);
	return 0;
}

int discover_addresses(int timeout){
	//====== send a discovery request ======
	ddp_response_t *responses;
	int count = ddp_query(timeout,DDP_REQUEST_DISCOVER_ADDRESSES,&responses);
	if (count < 0) return -1;
	for (ddp_response_t *response = responses; response != NULL; response = response->next){
		printf("%s\n",response->header.hostname);
		for (size_t i = 0; i < response->data_len/sizeof(struct sockaddr); i++){
			struct sockaddr *addr = response->data;
			printf("--> %s\n",sockaddr_to_string(addr+i));
		}
	}
	ddp_free_responses(responses);
	return 0;
}
