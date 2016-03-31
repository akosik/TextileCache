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
#include <errno.h>
#include "client.h"
#include "base64.h"
#include "jsmn/jsmn.h"

#define MAXLINE 1024

/*
Follows the cache.h api. Allows users to cache values of any type and any size, given that they specified a cache
large enough. Packets to and from server are base64 encoded and terminated by an unencoded carriage return and
newline as per HTTP standards. Usually this would be unacceptable if there were multiple implementations (HTTP
explicilty says not to rely on an ending CRLF token) but since the server and client are meant for each other
(awww) this should suffice for checking when a message has been fully received.
*/

//cache object
struct cache_obj
{
  char* host;
  char* port;
  struct addrinfo *info;
};

char *extract_value_from_json(char *json)
{
  jsmn_parser parser;
  jsmntok_t tokens[5] = {0};

  jsmn_init(&parser);
  int numtoks = jsmn_parse(&parser, json, strlen(json), tokens, 5);

  int valstart = tokens[4].start;
  int valend = tokens[4].end;

  int valsize = valend - valstart;

  char *buffer = calloc(valsize + 1,1);

  memcpy(buffer,&json[valstart],valsize);
  printf("%s\n",buffer);

  if(!strcmp(buffer,"NULL"))
    {
      free(buffer);
      return NULL;
    }

  return buffer;
}

//custom send function for sending buffers of arbitrarily large sizes (except for tcp limit)
void sendbuffer(int fd, char *buffer,uint32_t size)
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

  while( buffer[bytes] == '\0' )
    {
      bytes = read(fd,buffer,MAXLINE);
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

    return response;
}

//tiny helper function for printing out the host ip (ipv4/ipv6 agnostic)
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//helper function to establish a connection to the host
//does NOT call getaddrinfo, which must be supplied to the function
//through the cache struct
int establish_connection(cache_t cache)
{
  int socket_fd;
  char s[INET6_ADDRSTRLEN];

  if ( (socket_fd = socket(cache->info->ai_family, cache->info->ai_socktype, cache->info->ai_protocol)) == -1)
    {
      printf("socket error.\n");
      exit(1);
    }

  //printf("Connecting...\n");
  if ( connect(socket_fd, cache->info->ai_addr, cache->info->ai_addrlen) == -1)
    {
      printf("connection refused.\n");
      exit(1);
    }
  inet_ntop(cache->info->ai_family, get_in_addr((struct sockaddr *)cache->info->ai_addr), s, sizeof s);
  //printf("Client: connecting to %s\n", s);

  return socket_fd;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Create Cache
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
cache_t create_cache(uint64_t maxmem)
{
  //create local cache object
  cache_t cache = calloc(1,sizeof(struct cache_obj));
  extern char *hostname;
  extern char *port;
  cache->host = hostname;
  cache->port = port;
  cache->info = calloc(1,sizeof(struct addrinfo));
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int status;
  if( (status = getaddrinfo(cache->host, cache->port, &hints, &cache->info)) != 0)
    {
      printf("getaddrinfo error: %s\n", gai_strerror(status));
      freeaddrinfo(cache->info);
      free(cache);
      exit(1);
    }

  //connect to server
  int socket_fd = establish_connection(cache);

  //text and encoded buffers
  char sendbuff[50] = {0};
  sprintf(sendbuff,"POST /memsize/%llu",maxmem);
  printf("Client Request: %s\n",sendbuff);

  //send the encoded buffer
  sendbuffer(socket_fd,sendbuff,strlen(sendbuff) + 1);

  //recieve the response, decode, print and return
  char *recvbuff = recvbuffer(socket_fd);
  printf("Server Response: %s\n",recvbuff);

  free(recvbuff);

  close(socket_fd);
  return cache;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cache Set
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void cache_set(cache_t cache, key_type key, val_type val, uint32_t val_size)
{
  //establish connection
  int socket_fd = establish_connection(cache);

  //calculate encoded buffer size, allocate encoded buffer
  int buffsize = strlen(key) + strlen(val) + 10;
  char *sendbuff = calloc(buffsize,1);

  sprintf(sendbuff,"PUT /%s/%s",key,val);

  printf("Client Request: %s\n",sendbuff);

  //send and then free the used buffers
  sendbuffer(socket_fd,sendbuff,buffsize);

  free(sendbuff);

  //recieve the buffer, decode, print and return
  char *recvbuff = recvbuffer(socket_fd);
  //printf("Server Response: %s\n",decoded);

  free(recvbuff);

  close(socket_fd);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cache Get
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Retrieve the value associated with key in the cache, or NULL if not found
val_type cache_get(cache_t cache, key_type key, uint32_t *val_size)
{
  //establish connection
  int socket_fd = establish_connection(cache);

  //define buffer and encoded buffer specs
  //10 is arbitrary, just needs to be big enough for GET keyword
  int buffsize = strlen(key) + 10;
  char *sendbuff = calloc(buffsize,1);
  sprintf(sendbuff,"GET /%s",key);
  //printf("Client Request: %s\n",buffer);

  //send it off
  sendbuffer(socket_fd,sendbuff,buffsize);

  free(sendbuff);

  //recieve the buffer, decode, print and return
  char *recvbuff = recvbuffer(socket_fd);
  //printf("Server Response: %s\n",decoded);

  free(recvbuff);

  //Parse json and extract value
  char *ret = extract_value_from_json(recvbuff);
  if(ret != NULL)
    *val_size = strlen(ret) + 1;
  else
    *val_size = 0;

  close(socket_fd);
  return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cache Delete
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Delete an object from the cache, if it's still there
void cache_delete(cache_t cache, key_type key)
{
  //establish connection
  int socket_fd = establish_connection(cache);

  //declare buffers (10, again to hold HTTP keyword)
  int buffsize = strlen(key) + 10;
  char *sendbuff = calloc(buffsize,1);
  sprintf(sendbuff,"DELETE /%s",key);
  printf("Client Request: %s\n",sendbuff);

  //send
  sendbuffer(socket_fd,sendbuff,buffsize);
  free(sendbuff);

  //receive
  char *recvbuff = recvbuffer(socket_fd);
  printf("Server Response: %s\n",recvbuffer);

  free(recvbuff);

  close(socket_fd);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destroy Cache
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Destroy all resource connected to a cache object
void destroy_cache(cache_t cache)
{
  //establish connection
  int socket_fd = establish_connection(cache);

  //populate buffer and encode
  char *sendbuff = "POST /shutdown";
  printf("Client Request: %s\n",sendbuff);

  //send
  sendbuffer(socket_fd,sendbuff,strlen(sendbuff) + 1);

  //receive
  char *recvbuff = recvbuffer(socket_fd);
  printf("Server Response: %s\n",recvbuff);

  //free everything and close
  free(recvbuff);
  close(socket_fd);
  freeaddrinfo(cache->info);
  free(cache);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get Head
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void get_head(cache_t cache)
{
  int socket_fd = establish_connection(cache);

  char *sendbuff = "HEAD";
  printf("Client Request: %s\n",sendbuff);

  sendbuffer(socket_fd,sendbuff,strlen(sendbuff) + 1);

  char *recvbuff = recvbuffer(socket_fd);
  printf("Server Response: %s\n",recvbuff);

  free(recvbuff);

  close(socket_fd);
}
