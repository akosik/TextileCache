#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "cache.h"
#include <time.h>

char *hostname;
char *tcpport;
char *udpport;

void test_gets(uint8_t* keys, uint32_t* values, uint64_t numpairs)
{
  cache_t cache = create_cache(numpairs*10);

  struct timespec start, end;
  const uint64_t requests = numpairs;
  const double nsToSec = 1000000000;
  const uint32_t nsToms = 1000000;

  char **keystrings = calloc(numpairs,sizeof(char*));
  char **valstrings = calloc(numpairs,sizeof(char*));

  for(int i = 0; i < numpairs; ++i)
    {
      keystrings[i] = calloc(keys[i],1);
      valstrings[i] = calloc(values[i],1);
      memset(keystrings[i],'K',keys[i]);
      memset(valstrings[i],'V',values[i]);
      keystrings[i][keys[i] - 1] = '\0';
      valstrings[i][values[i] - 1] = '\0';
    }

  clock_gettime(CLOCK_MONOTONIC,&start);
  for(int i = 0; i < numpairs; ++i)
      cache_set(cache,keystrings[i],valstrings[i],values[i]);
  clock_gettime(CLOCK_MONOTONIC,&end);

  uint64_t duration = (end.tv_sec * nsToSec + end.tv_nsec) - (start.tv_sec * nsToSec + start.tv_nsec);

  uint64_t ns = duration;
  double time_elapsed_sec = (double) duration / nsToSec;

  double requests_per_second = (double) requests / time_elapsed_sec;
  double ms = (double) ns / (requests * nsToms);

  printf("Time per Get: %f milliseconds\n",ms);
  printf("Requests per second: %f requests\n",requests_per_second);

  for(int i = 0; i < numpairs; ++i)
    free(valstrings[i]);
  free(valstrings);
}


int main(int argc, char *argv[])
{
  hostname = "134.10.103.229";
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

  while (scanf("%"PRIu32,&v) == 1)
    {
      values[j++] = v;
      if( j >= numpairs )
        break;
    }

  test_gets(keys,values,numpairs); //udp test
}
