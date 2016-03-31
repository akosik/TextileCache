//Homework 4: Software Cache
//Alec Kosik

#include <stdio.h>
#include "cache.h"

typedef struct pair_t
{
  key_type key;
  val_type val;
  size_t size;
  node evict;
} pair;

struct cache_obj
{
  //The list of key-value pairs
  pair *dict;
  //The total number of bins in the dict, following c++ naming conventions
  size_t capacity;
  //Number of keys in the cache
  size_t length;
  //Size taken up by values
  size_t memsize;
  //Maximum Memory
  size_t maxmem;
  //hash function
  hash_func hash;
  //eviction struct
  evict_class *evict;
};

// Default hash known as djb2
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
  uint64_t hashval;
  key_type key;
  val_type val;
  node eviction;
  uint32_t size;
  pair *current;
  int j = 0;
  for(uint64_t i = 0; i < cache->capacity; ++i)
    {
      if(cache->dict[i].key != NULL)
        {
          //Rehash and probe, then reset everything
          hashval = cache->hash(cache->dict[i].key) % (cache->capacity * 2);
          for(current = &temp[hashval]; current->key != NULL; current = &temp[++hashval % (cache->capacity * 2)]) continue;
          current->key = calloc(strlen(cache->dict[i].key)+1,sizeof(uint8_t));
          strcpy(current->key,cache->dict[i].key);
          current->val = changeval(current->val,cache->dict[i].val,cache->dict[i].size);
          current->evict = cache->dict[i].evict;
          current->size = cache->dict[i].size;
          current->evict->tabindex = hashval % (cache->capacity * 2);
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
    }

  //Allocate eviction metadata
  cache->evict = calloc(1,sizeof(evict_class));
  if (cache->evict == NULL) exit(1);

  //Set metadata, length and memsize will be 0 from calloc
  cache->capacity = 100;
  cache->maxmem = maxmem;

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
  pair *current;

  // if the value is too big, don't even bother, tell the user to get more RAM
  if(val_size > cache->maxmem)
    {
      printf("That value is too big. It was not stored.\n");
      return;
    }

  // Remove values until the cache has enough room for the new value
  while(cache->memsize + val_size > cache->maxmem)
    {
      uint64_t index = cache->evict->remove(cache->evict);
      current = &cache->dict[index];
      free(current->key);
      free(current->val);
      current->key = NULL;
      current->val = NULL;
      cache->memsize -= current->size;
      --cache->length;
    }

  // hash the key and perform linear probing until an open spot is found.
  // Since the cache resizes, the cache should never be full.
  uint64_t hashval = cache->hash(key) % cache->capacity;
  for(current = &cache->dict[hashval]; current->key != NULL; current = &cache->dict[++hashval % cache->capacity])
    {
      //if the keys match, replace that pair
      if(!strcmp(current->key,key))
        {
          current->val = changeval(current->val,val,val_size);
          cache->memsize -= current->size;
          cache->memsize += val_size;
          current->size = val_size;
          cache->evict->add(cache->evict,current->evict,hashval % cache->capacity);
          return;
        }
    }
  //if you found an open spot, save the pair there
  current->key = calloc(strlen(key)+1,sizeof(uint8_t));
  strcpy(current->key,key);
  current->val = changeval(current->val,val,val_size);
  ++cache->length;
  current->size = val_size;
  cache->memsize += val_size;
  cache->evict->add(cache->evict,current->evict,hashval % cache->capacity);
  //resize if over half full
  if((cache->length / (float)cache->capacity) > .5) cache_resize(cache);
}

// Retrieve the value associated with key in the cache, or NULL if not found
val_type cache_get(cache_t cache, key_type key, uint32_t *val_size)
{
  //hash and check for a key match, else return NULL
  uint64_t n = 0;
  pair *current;
  uint64_t hashval = cache->hash(key) % cache->capacity;
  for(; n < cache->capacity; hashval = ++hashval % cache->capacity)
    {
      current = &cache->dict[hashval];
      if(current->key != NULL)
        {
          if(!strcmp(current->key,key))
            {
              cache->evict->add(cache->evict,current->evict,hashval);
              *val_size = current->size;
              return current->val;
            }
        }
      ++n;
    }
  return NULL;
}

// Delete an object from the cache, if it's still there
void cache_delete(cache_t cache, key_type key)
{
  uint64_t n = 0;
  pair *current;
  uint64_t hashval = cache->hash(key) % cache->capacity;
  for(; n < cache->capacity; hashval = ++hashval % cache->capacity)
    {
      current = &cache->dict[hashval];
      if(current->key != NULL)
        {
          if(!strcmp(current->key,key))
            {
              free(current->key);
              free(current->val);
              current->key = NULL;
              current->val = NULL;
              cache->evict->remove(cache->evict);
              --cache->length;
              cache->memsize -= current->size;
              return;
            }
        }
      ++n;
    }
}

// Compute the total amount of memory used up by all cache values (not keys)
uint64_t cache_space_used(cache_t cache)
{
  return cache->memsize;
}

// Destroy all resources connected to a cache object
// Destroy is not constant time but presumably you don't need speed here since
// you're tearing down your cache.
// you could just leave this function blank and let the OS free everything,
// then it would LOOK constant, but then hopefully you don't plan on keeping
// the program running.
void destroy_cache(cache_t cache)
{
  uint64_t i = 0;
  for( ; i < cache->capacity; ++i)
    {
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
