CC = gcc
CFLAGS = -Wall -Wextra -std=c11

all: server client

server: server.c user_auth.c auction_io.c queue.c bid_processor.c common.h queue.h bid_processor.h
	$(CC) $(CFLAGS) server.c user_auth.c auction_io.c queue.c bid_processor.c -o server -lpthread

client: client.c common.h
	$(CC) $(CFLAGS) client.c -o client

clean:
	rm -f server client

.PHONY: all clean
