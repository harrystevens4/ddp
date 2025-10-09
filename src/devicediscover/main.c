#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "../libddp/ddp.h"

int discover_addresses();

void print_help(){
	printf("devicediscover <command> [options]\n");
	printf("commands:\n");
	printf("	addresses : print the hostnames and ip addresses of all broadcasting devices\n");
	printf("	help     : show this text\n");
	printf("	ping     : ping a host to see if they respond\n");
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
		//ping();
		break;
	default:
		fprintf(stderr,"Unknown command \"%s\"\n",argv[optind]);
		return 1;
	}
	return 0;
}

int discover_addresses(int timeout){
	//====== start a timer ======
	struct timespec end_time;
	int result = clock_gettime(CLOCK_BOOTTIME,&end_time);
	if (result != 0){
		perror("clock_gettime");
		return -1;
	}
	end_time.tv_sec += timeout/1000;
	end_time.tv_nsec += (timeout%1000)*1000000;
	//====== send a discovery request ======
	int sockfd = ddp_new_socket(timeout);
	if (sockfd < 0) return -1;
	result = ddp_broadcast(sockfd,DDP_REQUEST_DISCOVER_ADDRESSES,NULL,0);
	if (result < 0) return -1;
	for (;;){
		//====== has timeout expired? ======
		struct timespec time_now;
		int result = clock_gettime(CLOCK_BOOTTIME,&time_now);
		if (result != 0){
			perror("clock_gettime");
			return -1;
		}
		if (time_now.tv_sec > end_time.tv_sec || (time_now.tv_nsec >= end_time.tv_nsec && time_now.tv_sec == end_time.tv_sec)) break;
		//====== get a response ======
		char *packet = NULL;
		struct sockaddr sender_addr;
		socklen_t sender_addrlen = sizeof(sender_addr);
		long packet_size = ddp_receive_response(sockfd,&packet,&sender_addr,&sender_addrlen);
		if (packet_size < 0) goto _continue;
		struct ddp_header *header = (struct ddp_header *)packet;
		if (header->type != DDP_RESPONSE_DISCOVER_ADDRESSES) goto _continue;
		printf("response from host \"%s\"\n",header->hostname);
		//====== print out all the addresses ======
		size_t addr_count = header->body_size/sizeof(struct sockaddr);
		struct sockaddr *addrs = (struct sockaddr *)(packet + sizeof(struct ddp_header));
		for (size_t i = 0; i < addr_count; i++){
			struct sockaddr *addr = addrs + i;
			printf("--> %s\n",sockaddr_to_string(addr));
		}
		_continue:
		free(packet);
	}
	return 0;
}
