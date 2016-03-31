//TCP Establish, Send and Receive

#include "tcp.h"
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

//tiny helper function for printing out the host ip (ipv4/ipv6 agnostic)
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int establish_tcp_server(char *tcpport)
{
  int socket_fd, listen_fd, status;
  struct addrinfo hints, *res;

  //setup tcp
  memset(&hints,0,sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ( (status = getaddrinfo(NULL,tcpport,&hints,&res)) != 0)
    {
      printf("getaddrinfo error: %s\n", gai_strerror(status));
      freeaddrinfo(res);
      exit(1);
    }

  socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

  if (bind(socket_fd, res->ai_addr, res->ai_addrlen) < 0)
    {
      printf("bind error.\n");
      exit(1);
    }

  freeaddrinfo(res);

  listen_fd = listen(socket_fd,10);

  printf("Listening on TCP port: %s \n",tcpport);

  return socket_fd;
}

//helper function to establish a connection to the host
//does NOT call getaddrinfo, which must be supplied to the function
//through the cache struct
int establish_tcp_client(cache_t cache)
{
  int socket_fd;
  char s[INET6_ADDRSTRLEN];

  if ( (socket_fd = socket(cache->tcpinfo->ai_family, cache->tcpinfo->ai_socktype, cache->tcpinfo->ai_protocol)) == -1)
    {
      printf("socket error.\n");
      exit(1);
    }

  printf("Connecting...\n");
  if ( connect(socket_fd, cache->tcpinfo->ai_addr, cache->tcpinfo->ai_addrlen) == -1)
    {
      printf("connection refused.\n");
      exit(1);
    }
  inet_ntop(cache->tcpinfo->ai_family, get_in_addr((struct sockaddr *)cache->tcpinfo->ai_addr), s, sizeof s);
  printf("Client: connecting to %s\n", s);

  return socket_fd;
}

//custom send function for sending buffers of arbitrarily large sizes (except for tcp limit)
void sendbuffer(int fd, char *buffer, uint32_t size)
{
  uint32_t total = 0,bytes = 0,leftToSend = size;
  while( total < size )
    {
      bytes = write(fd,buffer + total,leftToSend);

      if(bytes == -1)
        {
          printf("Send failed\n");
          exit(1);
        }
      total += bytes;
      leftToSend -= bytes;
    }
}

//handles multi-packet requests, assumes only one request will come at a time
char* recvbuffer(int fd)
{
  char buffer[MAXLINE] = {0};
  char *response = calloc(MAXLINE,1);
  uint32_t total = 0,
    response_size = MAXLINE,
    bytes = 0;
  char *tmp;

  do
    {
      bytes = read(fd,buffer,MAXLINE);
      printf("%s%d\n",buffer,buffer[bytes - 1]);
      if(bytes == -1)
        {
          printf("Read failed\n");
          exit(1);
        }
      if( total + bytes > response_size)
        {
          tmp = calloc(response_size*2,1);
          if (tmp == NULL)
            {
              printf("Allocation failed, value too big\n");
              exit(1);
            }
          memcpy(tmp,response,response_size);
          free(response);
          response = tmp;
          response_size *= 2;
        }
      memcpy(response + total,buffer,bytes);
      total += bytes;
    }
  while( buffer[bytes - 1] != '\0' );

  return response;
}
