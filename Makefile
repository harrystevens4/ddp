CFLAGS=-Wall -Wextra -g
all : ddpbeacon devicediscover
ddpbeacon : src/libddp/ddp.o src/ddpbeacon/main.o
	$(CC) -o $@ $^
devicediscover : src/devicediscover/main.o src/libddp/ddp.o
	$(CC) -o $@ $^
