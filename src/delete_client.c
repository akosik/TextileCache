#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include "cache.h"
char *hostname;
char *tcpport;
char *udpport;

struct cache_obj
{
  char* host;
  char* tcpport;
  char* udpport;
  struct addrinfo *tcpinfo;
  struct addrinfo *udpinfo;
};

void test_gets(uint8_t* keys, uint64_t numpairs)
{
  cache_t cache = create_cache(numpairs*10);
  char **keystrings = calloc(numpairs,sizeof(char*));

  for(int i = 0; i < numpairs; ++i)
    {
      keystrings[i] = calloc(keys[i],1);
      memset(keystrings[i],'K',keys[i]);
      keystrings[i][keys[i] - 1] = '\0';
    }
  uint32_t val_size = 0;

  uint64_t errors = 0;
  const uint64_t requests = numpairs;
  const double nsToSec = 1000000000;
  const uint32_t nsToms = 1000000;
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC,&start);

  for(int i = 0; i < requests; ++i)
    cache_delete(cache,keystrings[i]);

  clock_gettime(CLOCK_MONOTONIC,&end);
  uint64_t duration = (end.tv_sec * nsToSec + end.tv_nsec) - (start.tv_sec * nsToSec + start.tv_nsec);

  uint64_t ns = duration;
  double time_elapsed_sec = (double) duration / nsToSec;

  double requests_per_second = (double) requests / time_elapsed_sec;
  double ms = (double) ns / (requests * nsToms);

  printf("Time per Get: %f milliseconds\n",ms);
  printf("Requests per second: %f requests\n",requests_per_second);
}

int main(int argc, char *argv[])
{
  hostname = "134.10.103.234";
  tcpport = "2001";
  udpport = "3001";

  int i = 0,j = 0;
  uint8_t k;
  uint32_t v;
  uint64_t numpairs = atoi(argv[1]);
  uint8_t *keys = calloc(numpairs,sizeof(uint8_t));
  uint32_t *values = calloc(numpairs,sizeof(uint32_t));
  while (scanf("%"PRIu8,&k) == 1)
  {
    keys[i++] = k;
    if( i >= numpairs )
      break;     
  }

  test_gets(keys,numpairs); //udp test
}
