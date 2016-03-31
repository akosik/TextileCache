//TCP functions for Establish, Send and Receive
//supports large, broken messages

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

//returns file descriptor, does everything up to accept
int establish_tcp(char *tcpport);

void sendbuffer(int fd, char *buffer, uint32_t size);

//returns message received
char* recvbuffer(int fd);

//helper for printing out hostnames
void *get_in_addr(struct sockaddr *sa);
