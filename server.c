#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include "cache.h"
#include <string.h>
#include <time.h>
#include "base64.h"

#define MAXLINE 1024

//Parses client request into an http command, a primary argument, and a secondary argument
//Any of these fields can be omitted by passing NULL for that buffer
int parse_request(char *buffer, char *command, char *primary, char *secondary, uint64_t buffsize)
{
  int i,cpybufferindex = 0;
  for(i = 0; i < buffsize; ++i)
    {
      //printf("command: %c\n",buffer[i]);
      if( buffer[i] != ' ' && buffer[i] != '\0')
        {
          if (command != NULL)
            command[cpybufferindex++] = buffer[i];
        }
      else break;
    }

  if(primary != NULL || secondary != NULL)
    {
      if(buffer[++i] != '/')
        return -1;

      cpybufferindex = 0;
      for(++i ; i < buffsize; ++i)
        {
          //printf("primary: %c\n",buffer[i]);
          if(buffer[i] != '/' && buffer[i] != '\0')
            {
              if(primary != NULL)
                primary[cpybufferindex++] = buffer[i];
            }
          else break;
        }

      if(secondary != NULL)
        {
          if(buffer[i] != '/')
            return -1;

          cpybufferindex = 0;
          for(++i ; i < buffsize; ++i)
            {
              //printf("secondary: %c\n",buffer[i]);
              if(buffer[i] != '\0')
                {
                  if(secondary != NULL)
                    secondary[cpybufferindex++] = buffer[i];
                }
              else break;
            }
        }
    }
  return 1;
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
      printf("%s\n",buffer);
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

//handle session
cache_t handle_request(int fd, cache_t cache)
{
  //recieve client request
  char *request = recvbuffer(fd);
  int request_size = strlen(request) + 1;

  //decode and print request (binary data in request will not print)
  char command[10] = {0};
  char *primary = calloc(request_size,1);
    if(primary == NULL)
    {
      printf("Allocation Failed\n");
      return cache;
    }
  char *secondary = calloc(request_size,1);
    if(secondary == NULL)
    {
      printf("Allocation Failed\n");
      free(primary);
      return cache;
    }

  printf("%s\n",request);

  //get command
  parse_request(request,command,NULL,NULL,request_size);

  if (!strcmp(command,"GET"))
      {
        //get key and retrieve value
        parse_request(request,NULL,primary,NULL,request_size);
        uint32_t val_size = 0;
        char *ret = (char *)cache_get(cache,primary,&val_size);

        uint32_t json_length = val_size + strlen(primary) + 50;
        char *json = calloc(json_length,1);

        //populate with response and encode
        if(ret != NULL)
          sprintf(json,"{ \"key\" : \"%s\", \"val\" : \"%s\" }",primary,ret);
        else
          sprintf(json,"{ \"key\" : \"%s\", \"val\" : \"%s\" }",primary,"NULL");
        //printf("Response Request: %s\n",json);

        //send
        write(fd,json,strlen(json));
        free(json);
      }

    else if (!strcmp(command,"PUT"))
      {
        //get key and value and set in cache
        parse_request(request,NULL,primary,secondary,request_size);
        //printf("%llu, %s\n",cache_space_used(cache),secondary);
        cache_set(cache,primary,secondary,strlen(secondary) + 1);

        //ack back that the set was successful (so far as the server knows)
        const char *msg = "Value set in cache.\n";
        write(fd,msg,strlen(msg));
      }

    else if (!strcmp(command,"DELETE"))
      {
        //get key and delete associated value
        parse_request(request,NULL,primary,NULL,request_size);
        cache_delete(cache,primary);

        //ack back that the operation was completed
        const char *msg = "Value deleted.\n";
        write(fd,msg,strlen(msg) + 1);
      }

    else if (!strcmp(command,"HEAD"))
      {
        //get local time and print on server side
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        printf("now: %d-%d-%d %d:%d:%d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

        //get space used
        uint64_t space_used = cache_space_used(cache);

        char msg[MAXLINE] = {0};
        sprintf(msg,"Date: %d-%d-%d %d:%d:%d\nVersion: HTTP 1.1\nAccept: text/plain\nContent-Type: application/json\nSpace Used: %d\n",tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,space_used);
        write(fd,msg,strlen(msg) + 1);

      }

    else if (!strcmp(command,"POST"))
      {
        //get primary argument
        parse_request(request,NULL,primary,NULL,request_size);

        if(!strcmp(primary,"shutdown"))
          {
            //destroy the cache
            cache_destroy(cache);

            //ack back that the cache was cleared out
            const char *msg = "Clearing cache and ~existing cleanly~.\n";
            write(fd,msg,strlen(msg) + 1);

            return NULL;
          }
        else if(!strcmp(primary,"memsize"))
          {
            parse_request(request,NULL,NULL,secondary,request_size);
            uint64_t memsize = atoi(secondary);
            if(cache == NULL || cache_space_used(cache) == 0)
              {
                cache = create_cache(memsize);
                char msg[100] = {0};
                printf("Cache created with maxmem of %d.\n",memsize);
                sprintf(msg,"Cache created with maxmem of %d.\n",memsize);
                write(fd,msg,strlen(msg) + 1);
              }
            else
              {
                printf("Cache create called after initialization\n");
                char *msg = "400";
                write(fd,msg,strlen(msg) + 1);
              }
          }
      }

    else
      {
        const char *msg = "Malformed requested.\n";
        write(fd,msg,strlen(msg) + 1);

      }
  free(request);
  return cache;
}

int main(int argc, char *argv[])
{
  int maxmem;
  char *port;

  switch(argc)
    {
    case 1:
      maxmem = 100;
      port = "2001";
      break;
    case 3:
      if(!strcmp(argv[1],"-m"))
        {
          maxmem = atoi(argv[2]);
          port = "2001";
        }
      else if(!strcmp(argv[1],"-t"))
        {
          port = argv[2];
          maxmem = 100;
        }
      break;
    case 5:
      if(!strcmp(argv[1],"-m"))
        {
          maxmem = atoi(argv[2]);
          port = argv[4];
        }
      else if(!strcmp(argv[1],"-t"))
        {
          port = argv[2];
          maxmem = atoi(argv[4]);
        }
      break;
    default:
      printf("Usage: server -m <maxmem> -t <port#> \n");
      exit(1);
    }

  cache_t cache = create_cache(maxmem);

  int socket_fd, listen_fd, connfd, exitcode, status;
  struct sockaddr_in clientaddr;
  socklen_t clientaddr_size;
  struct addrinfo hints, *res;
  char *haddrp;
  struct hostent *hp;

  memset(&hints,0,sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ( (status = getaddrinfo(NULL,port,&hints,&res)) != 0)
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

  listen_fd = listen(socket_fd,8);

  printf("Listening on port: %s \n",port);

  while(1)
    {
      printf("Waiting for connection\n");
      connfd = accept(socket_fd, (struct sockaddr*) &clientaddr, &clientaddr_size);
      char ip4[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(clientaddr.sin_addr), ip4, INET_ADDRSTRLEN);
      printf("Connection with %s.\n", ip4);


      cache = handle_request(connfd,cache);
      close(connfd);
    }
  exit(0);
}
