CC = gcc
CFLAGS = -Wall -Wextra -std=c11

all: server client

server: server.c user_auth.c auction_io.c common.h
	$(CC) $(CFLAGS) server.c user_auth.c auction_io.c -o server

client: client.c common.h
	$(CC) $(CFLAGS) client.c -o client

clean:
	rm -f server client

.PHONY: all clean
