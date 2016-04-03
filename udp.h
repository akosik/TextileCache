//UDP functions Establish, Send and Receive
//with support for large, broken messages

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include "cache.h"

//Grabs and binds a socket, returns the socket fd
int establish_udp_server(char *udpport);

int establish_udp_client(cache_t cache);

int senddgrams(int fd, char *buffer, int size, struct sockaddr *to, socklen_t len);

//returns the message received
char* recvdgrams(int fd, struct sockaddr_storage *from);
