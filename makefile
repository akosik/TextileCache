CC = gcc

CFLAGS = -Wall -pedantic -Werror

server:
	$(CC) server.c cache.c lru.c tcp.c udp.c -o $@

client:
	$(CC) test.c testing.c client.c jsmn/jsmn.c tcp.c udp.c -o $@

clean_server:
	rm server

clean_client:
	rm client

clean:
	rm server client
