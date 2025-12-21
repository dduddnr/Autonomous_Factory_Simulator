CC = gcc
CFLAGS = -Wall
# 라이브러리 연결 (ncurses, pthread)
LIBS = -lncurses -lpthread

all: server client

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LIBS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LIBS)

clean:
	rm -f server client
