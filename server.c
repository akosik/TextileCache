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
#include "tcp.h"
#include "udp.h"

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




void handle_get(int fd, cache_t cache)
{
  struct sockaddr_storage client;
  int size = sizeof(struct sockaddr);

  char *request = recvdgrams(fd,&client);
  int request_size = strlen(request) + 1;

  char command[10] = {0};
  char *primary = calloc(request_size,1);
    if(primary == NULL)
    {
      printf("Allocation Failed\n");
    }

  printf("Request: %s\n",request);
  parse_request(request,command,primary,NULL,request_size);

  if (!strcmp(command,"GET"))
      {
        //get key and retrieve value
        uint32_t val_size = 0;
        char *ret = (char *)cache_get(cache,primary,&val_size);

        uint32_t json_length = val_size + strlen(primary) + 50;
        char *json = calloc(json_length,1);

        //populate with response and encode
        if(ret != NULL)
          sprintf(json,"{ \"key\" : \"%s\", \"val\" : \"%s\" }",primary,ret);
        else
          sprintf(json,"{ \"key\" : \"%s\", \"val\" : \"%s\" }",primary,"NULL");
        printf("Request Response: %s\n",json);

        //send
        senddgrams(fd,json,strlen(json) + 1,&client,size);
        free(json);
      }
  free(primary);
}


//handle session
cache_t handle_request(int fd, cache_t cache)
{
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

  if (!strcmp(command,"PUT"))
      {
        //get key and value and set in cache
        parse_request(request,NULL,primary,secondary,request_size);
        cache_set(cache,primary,secondary,strlen(secondary) + 1);

        //ack back that the set was successful (so far as the server knows)
        const char *msg = "Value set in cache.\n";
        sendbuffer(fd,msg,strlen(msg) + 1);
      }

    else if (!strcmp(command,"DELETE"))
      {
        //get key and delete associated value
        parse_request(request,NULL,primary,NULL,request_size);
        cache_delete(cache,primary);

        //ack back that the operation was completed
        const char *msg = "Value deleted.\n";
        sendbuffer(fd,msg,strlen(msg) + 1);
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
        sendbuffer(fd,msg,strlen(msg) + 1);

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
            sendbuffer(fd,msg,strlen(msg) + 1);

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
                sendbuffer(fd,msg,strlen(msg) + 1);
                //senddgrams(fd,msg,strlen(msg) + 1,client,sizeof(struct sockaddr));
              }
            else
              {
                printf("Cache create called after initialization\n");
                char *msg = "400";
                sendbuffer(fd,msg,strlen(msg) + 1);
              }
          }
      }

    else
      {
        const char *msg = "Malformed requested.\n";
        sendbuffer(fd,msg,strlen(msg) + 1);

      }
  //free(request);
  return cache;
}

int main(int argc, char *argv[])
{
  int maxmem = 100;
  char *tcpport = "2001";
  char *udpport = "3001";

  for(int i = 2;i < argc; ++i)
    {
      if(!strcmp(argv[i],"-m"))
        maxmem = atoi(argv[i+1]);
      else if (!strcmp(argv[i],"-t"))
        tcpport = argv[i+1];
      else if(!strcmp(argv[i],"-u"))
        udpport = argv[i+1];
    }

  cache_t cache = create_cache(maxmem);

  int tcp_fd = establish_tcp_server(tcpport);
  int udp_fd = establish_udp_server(udpport);

  struct sockaddr_in clientaddr;
  socklen_t clientaddr_size;
  char ip4[INET_ADDRSTRLEN];
  int connfd;

  fd_set readfds;
  FD_ZERO(&readfds);
  int fdmax = udp_fd < tcp_fd ? tcp_fd : udp_fd;
  int ret = 0;

  while(1)
    {
      FD_SET(tcp_fd, &readfds);
      FD_SET(udp_fd, &readfds);
      if ( (ret = select(fdmax+1, &readfds, NULL, NULL, NULL)) == -1)
        {
          printf("Select Error.\n");
          exit(1);
        }
      if(FD_ISSET(udp_fd,&readfds))
        {
          handle_get(udp_fd,cache);
          }

      if(FD_ISSET(tcp_fd, &readfds))
        {
          connfd = accept(tcp_fd, (struct sockaddr*) &clientaddr, &clientaddr_size);
          inet_ntop(AF_INET, &(clientaddr.sin_addr), ip4, INET_ADDRSTRLEN);
          printf("Connection with %s.\n", ip4);

          cache = handle_request(connfd,cache);
          close(connfd);
        }
    }
  exit(0);
}
