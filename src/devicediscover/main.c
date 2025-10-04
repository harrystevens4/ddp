#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "../libddp/ddp.h"

int discover();

void print_help(){
	printf("devicediscover <command> [options]\n");
	printf("commands:\n");
	printf("	discover : print the hostnames and ip addresses of all broadcasting devices\n");
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
	printf("%d\n",hash("ping"));
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
	case 47094: //help
		print_help();
		break;
	case 24008:
		discover();
		break;
	}
	case 0:
		ping();
		break;
	return 0;
}

int discover(){
	return 0;
}
