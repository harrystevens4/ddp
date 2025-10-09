CFLAGS=-Wall -Wextra -g
all : ddpd devicediscover
ddpd : src/libddp/ddp.o src/ddpd/main.o
	$(CC) -o $@ $^
devicediscover : src/devicediscover/main.o src/libddp/ddp.o
	$(CC) -o $@ $^
