CC = gcc

CFLAGS = -Wall -pedantic -Werror

server:
	$(CC) server.c cache.c lru.c base64.c -o $@

client:
	$(CC) test.c testing.c client.c base64.c jsmn/jsmn.c -o $@

clean_server:
	rm server

clean_client:
	rm client

clean:
	rm server client
