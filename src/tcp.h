//TCP functions for Establish, Send and Receive
//supports large, broken messages

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include "cache.h"

//returns file descriptor, does everything up to accept
int establish_tcp_server(char *tcpport);

int establish_tcp_client(cache_t cache);

void sendbuffer(int fd, char *buffer, uint32_t size);

//returns message received
char* recvbuffer(int fd);

//helper for printing out hostnames
void *get_in_addr(struct sockaddr *sa);
