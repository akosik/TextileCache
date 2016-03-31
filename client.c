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
#include "udp.h"
#include "tcp.h"

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
  char* tcpport;
  char* udpport;
  struct addrinfo *tcpinfo;
  struct addrinfo *udpinfo;
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Create Cache
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
cache_t create_cache(uint64_t maxmem)
{
  //create local cache object
  cache_t cache = calloc(1,sizeof(struct cache_obj));
  extern char *hostname;
  extern char *tcpport;
  extern char *udpport;
  cache->host = hostname;
  cache->tcpport = tcpport;
  cache->udpport = udpport;
  cache->tcpinfo = calloc(1,sizeof(struct addrinfo));
  cache->udpinfo = calloc(1,sizeof(struct addrinfo));

  struct addrinfo hints;
  int status;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  if ((status = getaddrinfo(cache->host, "3001", &hints, &cache->udpinfo)) != 0)
    {
      fprintf(stderr, "getaddrinfo for udp: %s\n", gai_strerror(status));
      freeaddrinfo(cache->udpinfo);
      free(cache);
      exit(1);
    }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if( (status = getaddrinfo(cache->host, cache->tcpport, &hints, &cache->tcpinfo)) != 0)
    {
      printf("getaddrinfo error for tcp: %s\n", gai_strerror(status));
      freeaddrinfo(cache->tcpinfo);
      free(cache);
      exit(1);
    }

  //connect to server
  int tcp_fd = establish_tcp_client(cache);

  //text and encoded buffers
  char sendbuff[50] = {0};
  sprintf(sendbuff,"POST /memsize/%llu",maxmem);
  printf("Client Request: %s\n",sendbuff);

  //send the encoded buffer
  sendbuffer(tcp_fd,sendbuff,strlen(sendbuff) + 1);

  //recieve the response, decode, print and return
  char *recvbuff = recvbuffer(tcp_fd);
  printf("Server Response: %s\n",recvbuff);

  free(recvbuff);

  close(tcp_fd);
  return cache;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cache Set
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void cache_set(cache_t cache, key_type key, val_type val, uint32_t val_size)
{
  //establish connection
  int socket_fd = establish_tcp_client(cache);

  //calculate encoded buffer size, allocate encoded buffer
  int buffsize = strlen(key) + strlen(val) + 10;
  char *sendbuff = calloc(buffsize,1);
  sprintf(sendbuff,"PUT /%s/%s",key,val);
  printf("Client Request: %s\n",sendbuff);

  //send and then free the used buffers
  sendbuffer(socket_fd,sendbuff,buffsize);
  printf("hello\n");

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
  int udpfd = establish_udp_client(cache);

  //define buffer and encoded buffer specs
  //10 is arbitrary, just needs to be big enough for GET keyword
  int buffsize = strlen(key) + 10;
  char *sendbuff = calloc(buffsize,1);
  sprintf(sendbuff,"GET /%s",key);
  printf("Client Request: %s\n",sendbuff);

  //send it off
  if ( senddgrams(udpfd,sendbuff,strlen(sendbuff) + 1,cache->udpinfo->ai_addr,cache->udpinfo->ai_addrlen) < 0)
    return NULL;

  free(sendbuff);

  //recieve the buffer, decode, print and return
  char *recvbuff = recvdgrams(udpfd,cache->udpinfo->ai_addr);
  printf("Server Response: %s\n",recvbuff);

  if(!strcmp(recvbuff,"Packet Lost 500"))
    return NULL;

  free(recvbuff);

  //Parse json and extract value
  char *ret = extract_value_from_json(recvbuff);
  if(ret != NULL)
    *val_size = strlen(ret) + 1;
  else
    *val_size = 0;

  close(udpfd);
  return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cache Delete
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Delete an object from the cache, if it's still there
void cache_delete(cache_t cache, key_type key)
{
  //establish connection
  int socket_fd = establish_tcp_client(cache);

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
  int socket_fd = establish_tcp_client(cache);

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
  freeaddrinfo(cache->tcpinfo);
  freeaddrinfo(cache->udpinfo);
  free(cache);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get Head
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void get_head(cache_t cache)
{
  int socket_fd = establish_tcp_client(cache);

  char *sendbuff = "HEAD";
  printf("Client Request: %s\n",sendbuff);

  sendbuffer(socket_fd,sendbuff,strlen(sendbuff) + 1);

  char *recvbuff = recvbuffer(socket_fd);
  printf("Server Response: %s\n",recvbuff);

  free(recvbuff);

  close(socket_fd);
}
