CFLAGS=-Wall -Wextra -g
ddpbeacon : src/libddp/ddp.o src/ddpbeacon/main.o
	$(CC) -o $@ $^
devicediscover : src/devicediscover/main.o
	$(CC) -o $@ $^
