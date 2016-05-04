CC = gcc

DEBUG = -g

CFLAGS = -Wall -pedantic -Werror

all:
	make server
	make client

server:
	$(CC) -std=gnu99 -pthread server.c cache.c lru.c tcp.c udp.c threadpool.c -o $@

client:
	$(CC) -std=gnu99 test.c testing.c client.c jsmn/jsmn.c tcp.c udp.c -o $@

sclean:
	rm server

cclean:
	rm client

debug: 
	$(CC) -std=gnu99 -pthread $(DEBUG) server.c cache.c lru.c tcp.c udp.c threadpool.c -o $@

clean:
	rm server client
