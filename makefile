CC = gcc -std=gnu99 -O3

DEBUG = -g

CFLAGS = -Wall -pedantic -Werror

SERVER_FILES = src/server.c src/cache.c src/lru.c src/tcp.c src/udp.c src/threadpool.c
CLIENT_FILES = src/client.c src/tcp.c src/udp.c jsmn/jsmn.c

all:
	make server 
	make set_client 
	make get_client
	make delete_client

server:
	$(CC) -pg -pthread $(SERVER_FILES) -o $@

set_client:
	$(CC) src/set_client.c $(CLIENT_FILES) -o $@
 
get_client:
	$(CC) src/get_client.c $(CLIENT_FILES) -o $@

delete_client:
	$(CC) src/delete_client.c $(CLIENT_FILES) -o $@

clean_server:
	rm server

clean_clients:
	rm set_client get_client delete_client

clean:
	rm server set_client get_client delete_client






