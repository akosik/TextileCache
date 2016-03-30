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
    bytes = 0,
    eot = 2;
  char *tmp;

  while( buffer[eot - 2] != '\r' && buffer[eot - 1] != '\n' )
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
      eot = bytes;
    }

    response[response_size - 2] = '\0';
    return response;
}

//handle session
cache_t handle_request(int fd, cache_t cache)
{
  //stack allocate transient buffers
  char *buffer = calloc(100000,1);
  char command[10] = {0};
  char primary[10000] = {0};
  char secondary[MAXLINE] = {0};
  char encoded[10000] = {0};

  //recieve client request
  char *request = recvbuffer(fd);

  //decode and print request (binary data in request will not print)
  printf("decoding...\n");
  Base64decode(buffer,request);
  printf("%s\n",buffer);

  //get command
  parse_request(buffer,command,NULL,NULL,MAXLINE);

  if (!strcmp(command,"GET"))
      {
        //get key and retrieve value
        parse_request(buffer,NULL,primary,NULL,MAXLINE);
        uint32_t val_size = 0;
        char *ret = (char *)cache_get(cache,primary,&val_size);

        //reuse old buffer
        memset(buffer,0,MAXLINE);

        //populate with response and encode
        if(ret != NULL)
          sprintf(buffer,"{ \"key\" : \"%s\", \"val\" : \"%s\" }",primary,ret);
        else
          sprintf(buffer,"{ \"key\" : \"%s\", \"val\" : \"%s\" }",primary,"NULL");
        printf("Response Buffer: %s\n",buffer);
        Base64encode(encoded,buffer,strlen(buffer) + 1);
        int endmark = strlen(encoded);
        encoded[endmark] = '\r';
        encoded[endmark + 1] = '\n';

        //send
        write(fd,encoded,strlen(encoded));
      }

    else if (!strcmp(command,"PUT"))
      {
        //get key and value and set in cache
        parse_request(buffer,NULL,primary,secondary,MAXLINE);
        cache_set(cache,primary,secondary,strlen(secondary) + 1);

        //ack back that the set was successful (so far as the server knows)
        const char *msg = "Value set in cache.\n";
        Base64encode(encoded,msg,strlen(msg) + 1);
        int endmark = strlen(encoded);
        encoded[endmark] = '\r';
        encoded[endmark + 1] = '\n';
        write(fd,encoded,strlen(encoded));
      }

    else if (!strcmp(command,"DELETE"))
      {
        //get key and delete associated value
        parse_request(buffer,NULL,primary,NULL,MAXLINE);
        cache_delete(cache,primary);

        //ack back that the operation was completed
        const char *msg = "Value deleted.\n";
        Base64encode(encoded,msg,strlen(msg) + 1);
        int endmark = strlen(encoded);
        encoded[endmark] = '\r';
        encoded[endmark + 1] = '\n';
        write(fd,encoded,strlen(encoded));
      }

    else if (!strcmp(command,"HEAD"))
      {
        memset(buffer,0,MAXLINE);

        //get local time and print on server side
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        printf("now: %d-%d-%d %d:%d:%d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

        //get space used
        uint64_t space_used = cache_space_used(cache);

        char msg[MAXLINE];
        sprintf(msg,"Date: %d-%d-%d %d:%d:%d\nVersion: HTTP 1.1\nAccept: text/plain\nContent-Type: application/json\nSpace Used: %d\n",tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,space_used);
        Base64encode(buffer,msg,strlen(msg) + 1);
        int endmark = strlen(buffer);
        buffer[endmark] = '\r';
        buffer[endmark + 1] = '\n';
        write(fd,buffer,strlen(buffer));
      }

    else if (!strcmp(command,"POST"))
      {
        //get primary argument
        parse_request(buffer,NULL,primary,NULL,MAXLINE);

        if(!strcmp(primary,"shutdown"))
          {
            //destroy the cache
            cache_destroy(cache);

            //ack back that the cache was cleared out
            const char *msg = "Clearing cache and ~existing cleanly~.\n";
            memset(buffer,0,MAXLINE);
            Base64encode(buffer,msg,strlen(msg) + 1);
            int endmark = strlen(buffer);
            buffer[endmark] = '\r';
            buffer[endmark + 1] = '\n';
            write(fd,buffer,strlen(buffer));
            return NULL;
          }
        else if(!strcmp(primary,"memsize"))
          {
            parse_request(buffer,NULL,NULL,secondary,MAXLINE);
            uint64_t memsize = atoi(secondary);
            if(cache == NULL || cache_space_used(cache) == 0)
              {
                cache = create_cache(memsize);
                char msg[MAXLINE] = {0};
                printf("Cache created with maxmem of %d.\n",memsize);
                sprintf(msg,"Cache created with maxmem of %d.\n",memsize);
                Base64encode(encoded,msg,strlen(msg) + 1);
                int endmark = strlen(encoded);
                encoded[endmark] = '\r';
                encoded[endmark + 1] = '\n';
                write(fd,encoded,strlen(encoded));
              }
            else
              {
                char msg[MAXLINE] = {0};
                printf("Cache create called after initialization\n");
                sprintf(msg,"%d.\n",400);
                Base64encode(encoded,msg,strlen(msg) + 1);
                int endmark = strlen(encoded);
                encoded[endmark] = '\r';
                encoded[endmark + 1] = '\n';
                write(fd,encoded,strlen(encoded));
              }
          }
      }

    else
      {
        const char *msg = "Malformed requested.\n";
        memset(buffer,0,MAXLINE);
        Base64encode(buffer,msg,strlen(msg) + 1);
        int endmark = strlen(buffer);
        buffer[endmark] = '\r';
        buffer[endmark + 1] = '\n';
        write(fd,buffer,strlen(buffer) + 1);
      }
  free(buffer);
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
