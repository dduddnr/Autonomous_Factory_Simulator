CC = gcc
CFLAGS = -Wall -DUSE_AUDIO
# ncurses, pthread, 그리고 SDL2(오디오) 라이브러리를 링크합니다.
LIBS = -lncurses -lpthread -lSDL2 -lSDL2_mixer

all: server client

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LIBS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LIBS)

clean:
	rm -f server client
