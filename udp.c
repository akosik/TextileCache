//Establish, Send and Receive For UDP
//Handles large, broken sends and receives

#include "udp.h"
#include "cache.h"

#define MAXLINE 1024

//cache object
struct cache_obj
{
  char* host;
  char* tcpport;
  char* udpport;
  struct addrinfo *tcpinfo;
  struct addrinfo *udpinfo;
};

int establish_udp_server(char *udpport)
{
  struct addrinfo hints, *res;

  //setup tcp
  memset(&hints,0,sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  int status;
  if ( (status = getaddrinfo(NULL,"3001",&hints,&res)) != 0)
    {
      printf("getaddrinfo error: %s\n", gai_strerror(status));
      freeaddrinfo(res);
      exit(1);
    }

  int socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

  //set socket timeout to 1000ms
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 1000;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
    perror("Error");
  }

  if (bind(socket_fd, res->ai_addr, res->ai_addrlen) < 0)
    {
      printf("bind error.\n");
      exit(1);
    }

  return socket_fd;
}

int establish_udp_client(cache_t cache)
{
  int socket_fd;
  if ( (socket_fd = socket(cache->udpinfo->ai_family, cache->udpinfo->ai_socktype, cache->udpinfo->ai_protocol)) == -1)
    {
      printf("socket error.\n");
      exit(1);
    }

  //set socket timeout to 1000ms
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 1000;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
    perror("Error");
  }

  return socket_fd;
}

int senddgrams(int fd, char *buffer, int size, struct sockaddr *to, socklen_t len)
{
  uint32_t total = 0,bytes = 0,leftToSend = size;
  while( total < size )
    {
      bytes = sendto(fd, buffer + total, leftToSend, 0, (struct sockaddr *)to, len);
      if(bytes == -1)
        {
          printf("Send failed\n");
          printf("Bytes sent: %d\n",total);
          return -1;
        }
      total += bytes;
      leftToSend -= bytes;
    }
  return 0;
}

char* recvdgrams(int fd, struct sockaddr_storage *from)
{
  char buffer[MAXLINE] = {0};
  char *response = calloc(MAXLINE,1);
  uint32_t total = 0,
    response_size = MAXLINE,
    bytes = 0;
  char *tmp;
  int size = sizeof(*from);
  do
    {
      bytes = recvfrom(fd,buffer,MAXLINE,0,(struct sockaddr *)from, &size);
      if(bytes == -1)
        {
          printf("Read failed\n");
	  free(response);
          return NULL;
        }
      if( total + bytes > response_size)
        {
          tmp = calloc(response_size*2,1);
          if (tmp == NULL)
            {
              printf("Allocation failed, value too big\n");
	      free(response);
              return NULL;
            }
          memcpy(tmp,response,response_size);
          free(response);
          response = tmp;
          response_size *= 2;
        }
      memcpy(response + total,buffer,bytes);
      total += bytes;
    } while( buffer[bytes] != '\0' );

    return response;
}
