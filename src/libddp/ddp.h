#ifndef _HOSTDP_H
#define _HOSTDP_H

#include <stdint.h>

//+--------------+                                  
//|    header    |                                  
//+--------------+                                  
//| ipv4 header 1|                                  
//+--------------+                                  
//| ipv4 header 2|                                  
//+--------------+                                  
//| ipv4 header n|                                  
//+--------------+                                  
//| ipv6 header 1|                                  
//+--------------+                                  
//| ipv6 header 2|                                  
//+--------------+                                  
//| ipv6 header n|                                  
//+--------------+                                  
//                                                  

#define DDP_PORT 7671
#define DDP_PORT_STRING "7671"
enum ddp_types { //requests are positive, responses negative and they match up
	//requests
	DDP_REQUEST_DISCOVER_ADDRESSES = 1,
	DDP_REQUEST_EMPTY = 2,
	//responses
	DDP_RESPONSE_DISCOVER_ADDRESSES = -1,
	DDP_RESPONSE_EMPTY = -2,
};

struct __attribute__((__packed__)) ddp_header {
	int8_t type; //discover_request response
	uint64_t body_size; //can be anything based on the requests
	char hostname[256];
};

int ddp_broadcast_discover(int sockfd);

#endif
