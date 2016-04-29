//Homework 4: Software Cache
//Alec Kosik

#include <stdio.h>
#include "cache.h"
#include <errno.h>

typedef struct pair_t
{
  pthread_rwlock_t lock;
  key_type key;
  val_type val;
  size_t size;
  node evict;
  uint32_t maxprobe;
} pair;

struct cache_obj
{
  //The list of key-value pairs
  pair *dict;
  //The total number of bins in the dict, following c++ naming conventions
  pthread_rwlock_t caplock;
  size_t capacity;
  //Number of keys in the cache
  pthread_rwlock_t lenlock;
  size_t length;
  //Size taken up by values
  pthread_rwlock_t memlock;
  size_t memsize;
  //Maximum Memory
  size_t maxmem;
  //hash function
  hash_func hash;
  //eviction struct
  evict_class *evict;
};

// Default hash: djb2
uint64_t defaultHash(key_type str)
{
  uint64_t hash = 5381;
  uint8_t c;

  while ((c = *str++))
    hash = ((hash << 5) + hash) + c;

  return hash;
}

// helper for modifying the value of a key-value pair
void *changeval(void *cacheval, const void *newval, size_t val_size)
{
  if(cacheval != NULL) free(cacheval);
  cacheval = calloc(val_size,sizeof(uint8_t));
  if(cacheval == NULL) exit(1);
  return memcpy(cacheval,newval,val_size);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cache Operations
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Allocates a block of memory twice the size of the original and copies
// all data into the new cache
void cache_resize(cache_t cache)
{
  printf("Resizing...\n");
  //Lock cache
  for(uint64_t step = 0; step < cache->capacity; ++step)
      pthread_rwlock_wrlock(&cache->dict[step].lock);

  //Reallocate
  pair *temp = NULL;
  temp = calloc(cache->capacity * 2, sizeof(pair));
  if(temp == NULL)
    {
      printf("Unable to resize cache. Your cache is doomed.\n");
      exit(1);
    }

  //Rehash old keys into new spots in new table and free the old table
  //this is obviously not constant time but resize is rare and when amortized over all
  //requests it does work out to be constant time
  pair *current, *hashpair;
  int j = 0;
  int probelength = 0;
  uint64_t hashval;
  for(uint64_t i = 0; i < cache->capacity; ++i)
    {
      if(cache->dict[i].key != NULL)
        {
          //Rehash and probe, then reset everything into the new cache
	  //No locks needed since resize halts all operations (and no one know about this cache yet)
          hashval = cache->hash(cache->dict[i].key) % (cache->capacity * 2);
	  hashpair = &temp[hashval];
          for(current = &temp[hashval]; current->key != NULL; current = &temp[++hashval % (cache->capacity * 2)]) ++probelength;
          current->key = calloc(strlen(cache->dict[i].key)+1,sizeof(uint8_t));
          strcpy(current->key,cache->dict[i].key);
          current->val = changeval(current->val,cache->dict[i].val,cache->dict[i].size);
          current->evict = cache->dict[i].evict;
          current->size = cache->dict[i].size;
          current->evict->tabindex = hashval % (cache->capacity * 2);
	  hashpair->maxprobe = probelength > hashpair->maxprobe ? probelength : hashpair->maxprobe;
          free(cache->dict[i].key);
          free(cache->dict[i].val);
          cache->dict[i].key = NULL;
        }
    }
  free(cache->dict);
  cache->dict = temp;

  //Allocate space for eviction structs for empty slots
  for(uint64_t i = 0; i < cache->capacity * 2; ++i)
    {
      if(cache->dict[i].key == NULL)
        {
          cache->dict[i].val = NULL;
          cache->dict[i].evict = calloc(1,sizeof(struct node_t));
        }
    }

  cache->capacity *= 2;
}

// Create a new cache object with a given maximum memory capacity.
// Pass NULL for last 3 functions for defaults
cache_t create_cache(uint64_t maxmem)
{
  //Allocate cache
  cache_t cache = calloc(1,sizeof(struct cache_obj));
  if (cache == NULL) exit(1);

  //Allocate list of key-value pairs
  cache->dict = calloc(100,sizeof(pair));
  if (cache->dict == NULL) exit(1);

  //Allocate eviction structs
  uint64_t i = 0;
  for( ; i < 100; ++i)
    {
      cache->dict[i].key = NULL;
      cache->dict[i].val = NULL;
      cache->dict[i].evict = calloc(1,sizeof(struct node_t));
      pthread_rwlock_init(&cache->dict[i].lock,NULL);
    }

  //Allocate eviction metadata
  cache->evict = calloc(1,sizeof(evict_class));
  if (cache->evict == NULL) exit(1);

  //Set metadata, length and memsize will be 0 from calloc
  cache->capacity = 100;
  pthread_rwlock_init(&cache->caplock,NULL);
  pthread_rwlock_init(&cache->lenlock,NULL);
  pthread_rwlock_init(&cache->memlock,NULL);
  cache->maxmem = maxmem;
  printf("Maxmem: %d,%d\n",maxmem,cache->maxmem);

  //Set function pointers
  cache->hash = defaultHash;
  cache->evict->add = lru_add;
  cache->evict->remove = lru_remove;

  return cache;
}

// Add a <key, value> pair to the cache.
// If key already exists, it will overwrite the old value.
// If maxmem capacity is exceeded, sufficient values will be removed
// from the cache to accomodate the new value.
void cache_set(cache_t cache, key_type key, val_type val, uint32_t val_size)
{
  printf("Maxmem: %d\n",cache->maxmem);
  // if the value is too big, don't even bother, tell the user to get more RAM
  if(val_size > cache->maxmem)
    {
      printf("That value is too big. It was not stored.\n");
      return;
    }

  pair *current;
  uint32_t old_val_size = 0;

  pthread_rwlock_rdlock(&cache->caplock);
  uint64_t hashval = cache->hash(key) % cache->capacity;
  pthread_rwlock_rdlock(&cache->dict[hashval].lock);
  uint32_t probelength = cache->dict[hashval].maxprobe;
  pthread_rwlock_unlock(&cache->dict[hashval].lock);

  uint64_t curr_index;
  for(int i = 0; i <= probelength; ++i )
    {
      curr_index = ((hashval + i) % cache->capacity);
      pthread_rwlock_rdlock(&cache->dict[curr_index].lock);
      if(cache->dict[curr_index].key != NULL)
	{
	  if(!strcmp(cache->dict[curr_index].key,key))
	    {
	      old_val_size = cache->dict[curr_index].size;
	      pthread_rwlock_unlock(&cache->dict[curr_index].lock);
	      break;
	    }
	}
      pthread_rwlock_unlock(&cache->dict[curr_index].lock);
    }

  // Remove values until the cache has enough room for the new value
  pthread_rwlock_rdlock(&cache->memlock);
  printf("Memsize: %d, Maxmem: %d\n",cache->memsize,cache->maxmem);
  while((cache->memsize + val_size - old_val_size) > cache->maxmem)
    {
      uint64_t index = cache->evict->remove(cache->evict);
      current = &cache->dict[index];
      
      pthread_rwlock_wrlock(&current->lock);
      if (current->key != NULL)
	{
	  free(current->key);
	  free(current->val);
	  current->key = NULL;
	  current->val = NULL;

	  pthread_rwlock_unlock(&cache->memlock);
	  pthread_rwlock_wrlock(&cache->memlock);
	  cache->memsize -= current->size;
	  pthread_rwlock_unlock(&cache->memlock);
	  pthread_rwlock_rdlock(&cache->memlock);

	  pthread_rwlock_wrlock(&cache->lenlock);
	  --cache->length;
	  pthread_rwlock_unlock(&cache->lenlock);
	}
      pthread_rwlock_unlock(&current->lock);
    }
  pthread_rwlock_unlock(&cache->memlock);

  // hash the key and perform linear probing until an open spot is found.
  // Since the cache resizes, the cache should never be full.
  pair *hashpair = &cache->dict[hashval];
  pair *tentative_abode = NULL;
  uint32_t tentative_hashval;
  uint32_t i = 0;
  //hash to the designated spot and then probe to the farthest spot any collided key has probed to before you!
  for(current = &cache->dict[hashval]; i <= probelength; current = &cache->dict[++hashval % cache->capacity])
    {
      ++i;
      pthread_rwlock_rdlock(&current->lock);
      if(current->key == NULL) 
	{
	  if(tentative_abode == NULL)
	    {
	      tentative_abode = current;
	      tentative_hashval = hashval % cache->capacity;
	    }
	  pthread_rwlock_unlock(&current->lock);
	  continue;
	}
      //if the keys match, replace that pair
      if(!strcmp(current->key,key))
        {
	  pthread_rwlock_unlock(&current->lock);
	  pthread_rwlock_wrlock(&current->lock);

          current->val = changeval(current->val,val,val_size);

	  pthread_rwlock_wrlock(&cache->memlock);
          cache->memsize -= current->size;
          cache->memsize += val_size;
	  pthread_rwlock_unlock(&cache->memlock);

          current->size = val_size;
          cache->evict->add(cache->evict,current->evict,hashval % cache->capacity);
	  pthread_rwlock_unlock(&cache->caplock);
	  pthread_rwlock_unlock(&current->lock);
          return;
        }
      pthread_rwlock_unlock(&current->lock);
    }

  //if you do not find yourself, TURN BACK, and set up shop in the first empty hashbucket you can find
  if(tentative_abode != NULL)
    {
      current = tentative_abode;
      pthread_rwlock_wrlock(&current->lock);
      hashval = tentative_hashval;
    }
  else
    {
      for( pthread_rwlock_wrlock(&current->lock) ; current->key != NULL; current = &cache->dict[++hashval % cache->capacity], pthread_rwlock_wrlock(&current->lock) )
	{
	  pthread_rwlock_unlock(&current->lock);
	  ++i;
	}
    }

  if(hashpair->maxprobe < i) hashpair->maxprobe = i;

  current->key = calloc(strlen(key)+1,sizeof(uint8_t));
  strcpy(current->key,key);
  current->val = changeval(current->val,val,val_size);

  pthread_rwlock_wrlock(&cache->lenlock);
  ++cache->length;
  pthread_rwlock_unlock(&cache->lenlock);

  current->size = val_size;

  pthread_rwlock_wrlock(&cache->memlock);
  cache->memsize += val_size;
  pthread_rwlock_unlock(&cache->memlock);

  cache->evict->add(cache->evict,current->evict,hashval % cache->capacity);
  pthread_rwlock_unlock(&current->lock);

  //resize if over half full
  pthread_rwlock_rdlock(&cache->lenlock);
  if((cache->length / (float)cache->capacity) > .5) 
      cache_resize(cache);

  pthread_rwlock_unlock(&cache->lenlock);
  pthread_rwlock_unlock(&cache->caplock);
}

// Retrieve the value associated with key in the cache, or NULL if not found
val_type cache_get(cache_t cache, key_type key, uint32_t *val_size)
{
  //hash and check for a key match, else return NULL
  uint64_t n = 0;
  pair *current;

  pthread_rwlock_rdlock(&cache->caplock);
  uint64_t hashval = cache->hash(key) % cache->capacity;
  uint32_t maxprobe = cache->dict[hashval].maxprobe;
  for(; n <= maxprobe; hashval = ++hashval % cache->capacity)
    {
      current = &cache->dict[hashval];
      pthread_rwlock_rdlock(&current->lock);
      if(current->key != NULL)
        {
          if(!strcmp(current->key,key))
            {
	      //switch to write lock
	      pthread_rwlock_unlock(&current->lock);
	      pthread_rwlock_wrlock(&current->lock);
              cache->evict->add(cache->evict,current->evict,hashval);
              *val_size = current->size;
	      pthread_rwlock_unlock(&cache->caplock);
	      pthread_rwlock_unlock(&current->lock);
              return current->val;
            }
        }
      pthread_rwlock_unlock(&current->lock);
      ++n;
    }
  pthread_rwlock_unlock(&cache->caplock);
  return NULL;
}

// Delete an object from the cache, if it's still there
void cache_delete(cache_t cache, key_type key)
{
  uint64_t n = 0;
  pair *current;

  pthread_rwlock_rdlock(&cache->caplock);
  uint64_t hashval = cache->hash(key) % cache->capacity;
  uint32_t maxprobe = cache->dict[hashval].maxprobe;
  for(; n <= maxprobe; hashval = ++hashval % cache->capacity)
    {
      current = &cache->dict[hashval];
      pthread_rwlock_rdlock(&current->lock);
      if(current->key != NULL)
        {
          if(!strcmp(current->key,key))
            {
	      //switch to write lock
	      pthread_rwlock_unlock(&current->lock);
	      pthread_rwlock_wrlock(&current->lock);
              free(current->key);
              free(current->val);
              current->key = NULL;
              current->val = NULL;
              cache->evict->remove(cache->evict);

	      pthread_rwlock_wrlock(&cache->lenlock);
              --cache->length;
	      pthread_rwlock_unlock(&cache->lenlock);

	      pthread_rwlock_wrlock(&cache->memlock);
              cache->memsize -= current->size;
	      pthread_rwlock_unlock(&cache->memlock);

	      pthread_rwlock_unlock(&cache->caplock);
	      pthread_rwlock_unlock(&current->lock);
              return;
            }
        }
      pthread_rwlock_unlock(&current->lock);
      ++n;
    }
}

// Compute the total amount of memory used up by all cache values (not keys)
uint64_t cache_space_used(cache_t cache)
{
  pthread_rwlock_rdlock(&cache->memlock);
  uint64_t ret = cache->memsize;
  pthread_rwlock_unlock(&cache->memlock);
  return ret;
}

void destroy_cache(cache_t cache)
{
  uint64_t i = 0;
  pthread_rwlock_rdlock(&cache->caplock);
  for( ; i < cache->capacity; ++i)
    {
      pthread_rwlock_wrlock(&cache->dict[i].lock);
      if(cache->dict[i].key != NULL)
        {
          free(cache->dict[i].key);
          free(cache->dict[i].val);
        }
      free(cache->dict[i].evict);
    }
  free(cache->dict);
  free(cache->evict);
  free(cache);
}

void print_cache(cache_t cache)
{
  for(int i = 0; i < cache->capacity; ++i)
    {
      if(cache->dict[i].key != NULL) printf("key: %s,val: %d, evict address: %d\n",cache->dict[i].key,*(uint64_t*)cache->dict[i].val,cache->dict[i].evict);
      else printf("evict address: %d\n",cache->dict[i].evict);
    }
}
