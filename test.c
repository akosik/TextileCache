#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <mach/mach_time.h>
#include "testing.h"
#include "client.h"

char *hostname;
char *tcpport;
char *udpport;

//Initializes cache to maxmem of 255
cache_t init()
{
  cache_t cache = create_cache(255);
  return cache;
}

//Sets 4 values and then gets all 4, checks if the values were set correctly
void set_multiple()
{
  cache_t cache = init();
  const uint8_t
    *key0 = (const uint8_t*) "hello",
    *key1 = (const uint8_t*) "thenumber3",
    *key2 = (const uint8_t*) "goodbye",
    *key3 = (const uint8_t*) "wow";
  uint8_t
    *value0 = "hello",
    *value1 = "how are you";
  uint8_t *value2 = "good thanks";
  uint8_t *value3 = "me too";

  cache_set(cache,key0,value0,strlen(value0) + 1);
  cache_set(cache,key1,value1,strlen(value1) + 1);
  cache_set(cache,key2,value2,strlen(value2) + 1);
  cache_set(cache,key3,value3,strlen(value3) + 1);

  uint32_t val_size = 0;

  uint8_t
    *val1 = (uint8_t*)cache_get(cache,key0,&val_size),
    *val2 = (uint8_t*)cache_get(cache,key1,&val_size);
  uint8_t *val3 = (uint32_t*)cache_get(cache,key2,&val_size);
  uint8_t *val4 = (uint64_t*)cache_get(cache,key3,&val_size);

  test(!strcmp(val1,"hello") && !strcmp(val2,"how are you") && !strcmp(val3,"good thanks") && !strcmp(val4,"me too"), "cache_set stores (multiple) values that are accessible");

  free(val1);
  free(val2);
  free(val3);
  free(val4);

  destroy_cache(cache);
}

// Hash function borrowed from Alex Ledger's implementation
uint64_t our_modified_jenkins(key_type key)
{
    // https://en.wikipedia.org/wiki/Jenkins_hash_function
    uint32_t hash = *key;
    hash += (hash << 10);
    hash ^= (hash >> 6);
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return (uint64_t) hash;
}

//Checks if a custom hash at least doesn't crash the cache (since some didn't have this customization)
//Sets a value with the custome hash and gets it to see if it was set properly
void custom_hash()
{
    uint8_t key[2] = {'a', '\0'};
    uint8_t value[6] = "12345";
    uint32_t val_size = 0;
    cache_t cache = create_cache(100 * sizeof(value));

    cache_set(cache, key, value, sizeof(value));
    uint8_t *ret = (uint8_t*)cache_get(cache, key, &val_size);
    test(!strcmp(key,"12345"), "cache_get works when given a custom hash (doesn't have to use custom hash)");

    destroy_cache(cache);
}

//Tests if cache returns val_size from cache
//sets a value and checks if the val_size from get is equal to the set value size
void test_get_valsize()
{
    uint8_t key[2] = {'a', '\0'};
    uint8_t *value = "size me";
    uint32_t val_size = 0;
    cache_t cache = create_cache(100 * sizeof(value));

    cache_set(cache, key, value, strlen(value) + 1);
    cache_get(cache, key, &val_size);
    test(val_size == 8, "cache_get sets val_size pointer");
    printf("%d\n",val_size);

    destroy_cache(cache);
}

//cache has empty memsize used right after creation
//creates a cache and checks memused
/*void test_empty_size()
{
    cache_t cache = create_cache(1024);
    uint64_t space = cache_space_used(cache);
    test(space == 0, "Empty cache uses zero space");

    destroy_cache(cache);
}
*/

//cache updates memsize after cache_set
//creates a cache, sets a value, and checks if memsize increased
/*void test_size()
{
    cache_t cache = create_cache(1024);
    uint8_t key[2] = {'a', '\0'};
    uint8_t value[6] = {10,11,12,13,14,15};
    uint32_t val_size = 0;
    cache_set(cache, key, value, sizeof(value));

    uint64_t space = cache_space_used(cache);
    test(space == sizeof(value), "Cache computes first value size correctly");

    destroy_cache(cache);
}
*/

//cache decrements memsize used after a key is removed
//same as above but it then deletes the value and sees if the memsize decreased
/*void test_size_after_delete()
{
    cache_t cache = create_cache(1024);
    uint8_t key[2] = {'a', '\0'};
    uint8_t value[6] = {10,11,12,13,14,15};
    uint32_t val_size = 0;
    cache_set(cache, key, value, sizeof(value));

    cache_delete(cache, key);

    uint64_t space = cache_space_used(cache);
    test(space == 0, "Cache size is zero after deleting all entries");

    destroy_cache(cache);
}
*/

//cache evicts keys to make room for new ones (jmcosel has a memsize min of 64)
//creates a cache with memsize of 64 and sets 4 values so that the 3rd will
//cause an eviction (if they didn't mess with the maxmem)
void eviction_couple()
{
  cache_t cache = create_cache(64);
  key_type
    key0 = (const uint8_t*) "hello",
    key1 = (const uint8_t*) "thenumber3",
    key2 = (const uint8_t*) "goodbye",
    key3 = (const uint8_t*) "wow";
  uint8_t
    *value0 = "a",
    *value1 = "b";
  uint8_t *chararray = "01234567890123456789012345678901234567890123456789012345678";
  uint8_t *value3 = "c";

  cache_set(cache,key0,value0,strlen(value0) + 1);
  cache_set(cache,key1,value1,strlen(value1) + 1);
  cache_set(cache,key2,chararray,60);
  cache_set(cache,key3,value3,strlen(value3) + 1);

  uint32_t val_size = 0;
  uint8_t *val = (uint8_t*) cache_get(cache,key3,&val_size);
  test(!strcmp(val,"c"),"keys are evicted to make space for new values");
  destroy_cache(cache);
}

//sets 4 values, gets the first value set, and sets a new value
//big enough to force an eviction, checks if the second key (least recently used)
//is removed, the third is removed, and finally if the first value added is still there
void evict_after_get()
{
  cache_t cache = create_cache(64);
  key_type
    key0 = (const uint8_t*) "hello",
    key1 = (const uint8_t*) "thenumber3",
    key2 = (const uint8_t*) "goodbye",
    key3 = (const uint8_t*) "wow";
  uint8_t
    *value0 = "1",
    *value1 = "3";
  uint8_t value2[60] = "01234567890123456789012345678901234567890123456789012345678";
  uint8_t *value3 = "123123124";

  cache_set(cache,key0,value0,strlen(value0) + 1);
  cache_set(cache,key1,value1,strlen(value1) + 1);
  cache_set(cache,key2,value2,60);

  uint32_t val_size = 0;
  uint8_t *val;

  // access first input
  val = (uint8_t*) cache_get(cache,key0,&val_size);

  // Set something that will require an eviction
  cache_set(cache,key3,value3,strlen(value3) + 1);

  // now get the last used value
  uint8_t *val0 = (uint8_t*) cache_get(cache,key0,&val_size);
  uint8_t *val1 = (uint8_t*) cache_get(cache,key1,&val_size);
  uint8_t *val2 = (uint8_t*) cache_get(cache,key2,&val_size);
  uint8_t *val3 = (uint8_t*) cache_get(cache,key3,&val_size);

  printf("%s,%s,%s,%s\n",val0,val1,val2,val3);

  test(!strcmp(val0,"1") && val1 == NULL && val2 == NULL && !strcmp(val3,"123123124"),"Last accessed key is evicted");
  destroy_cache(cache);
}

//test struct for next test
struct test_struct
{
  uint8_t *word;
  uint32_t num;
  uint8_t *stuff;
};

//cache handles more complex val types
//creates a struct, sets it in the cache, and checks if all of its elements are there
void struct_set()
{
  cache_t cache = init();
  key_type keystruct = (const uint8_t*) "struct";
  struct test_struct value4 = { .word = "it's a bag of words!", .num = 42, .stuff = NULL};
  uint32_t *size = calloc(1,sizeof(uint32_t));

    cache_set(cache,keystruct,&value4,sizeof(struct test_struct));

    struct test_struct *holder = (struct test_struct*) cache_get(cache,keystruct,size);

    uint8_t *val5 = holder->word;
    uint32_t val6 = holder->num;
    uint8_t *val7 = holder->stuff;

    test(!strcmp(val5,"it's a bag of words!") && val6 == 42 && val7 == NULL,"cache stores structs appropriately");
    free(size);
    destroy_cache(cache);
}

//cache updates value if new key inserted is the same as one already in the cache
//inserts two values with the same key and checks if the value was updated
void get_modified()
{
  cache_t cache = init();
  key_type key = (const uint8_t*) "hello";
  uint8_t *val1 = "201";
  uint8_t *val2 = "53";
  uint32_t size = 0;

  cache_set(cache,key,val1,strlen(val1) + 1);
  cache_set(cache,key,val2,strlen(val2) + 1);
  uint8_t *testval = (uint8_t*)cache_get(cache,key,&size);

  test(!strcmp(val2,"53"),"cache updates values");
  free(testval);
  destroy_cache(cache);
}

//cache returns NULL for key that isn't in the cache
//creates a cache and gets a key not in the cache, checks if it's null
void get_nonexistent()
{
  cache_t cache = init();
  key_type key = (const uint8_t*) "sartre";
  uint32_t size = 0;
  char *ret = (char*)cache_get(cache,key,&size);
  test(ret == NULL,"cache returns NULL for gets for keys not in cache");
  free(ret);
  destroy_cache(cache);
}

//cache resizes properly (this does not check evict! (it took some time to isolate these tests) )
//creates 100000 byte cache, malloc's a 100000 byte "String" iterates to 10000 and saves the iteration number
//with a key formed from i concatenations of "h"
//checks if the last value inserted is correct, mostly testing to see if it crashes or not
void resize()
{
  cache_t cache = create_cache(100000);
  uint64_t i = 1;
  char *key = calloc(10001,1);
  if (key == NULL)
    {
      printf("Allocation failed\n");
      exit(1);
    }
  for(; i <= 10000; ++i)
    {
      //printf("Before: %s,%d\n",key,key);
      strcat(key,"h");
      //printf("After:  %s\n",key);
      cache_set(cache,key,key,strlen(key) + 1);
      //printf("AfterS: %s\n",key);
      //for (int j = 0; j < 50; ++j) printf("%d ",key[j]);

    }
  uint32_t val_size = 0;
  char *val = (char*)cache_get(cache,key,&val_size);
  test(val != NULL && !strcmp(val,key),"cache resizes without failure (initial table size checked, all should resize, none should evict)");
  printf("%s\n",val);
  free(key);
  free(val);
  destroy_cache(cache);
}

//cache does not insert values that are too big for the cache
//inserts a value that is larger than the size of the cache
void val_too_big()
{
  cache_t cache = create_cache(90);
  key_type key = (const uint8_t*) "big";
  char large[100] = "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678";
  cache_set(cache,key,large, 100);
  uint32_t val_size = 0;
  char *ret = (char*)cache_get(cache,key,&val_size);
  test(ret == NULL,"cache doesn't save values that are too big to fit in the user specified mem space");
  destroy_cache(cache);
}

//cache returns memused size == to 0 if the val being inserted is too big for the cache (the cache memsize == 0 if the cache didnt increase memsize
//because the value is too big for the cache) this test was ignored if val_too_big just segfaulted
/*void cache_does_not_change_maxmem()
{
  cache_t cache = create_cache(90);
  key_type key = "big";
  char large[91];
  cache_set(cache,key,large, 91);
  uint64_t memused = cache_space_used(cache);
  test(memused == 0,"cache does not change user specified maxmem bound");
  destroy_cache(cache);
}
*/

//cache doesn't evict any keys if the new val is replacing one in the cache such that memsize is never exceeded
//sets a value of size 80 bytes, inserts a value with the same key of size 8 bytes
//checks if the least recently used value is still there (no evictions were performed)
void val_too_big_but_replacing()
{
  cache_t cache = create_cache(82);
  key_type standin = (const uint8_t*) "i shouldnt get removed";
  uint8_t *standinval = "34";
  cache_set(cache,standin,standinval,strlen(standinval) + 1);
  key_type key = (const uint8_t*) "doppleganger";
  uint8_t string[80] = "0123456789012345678901234567890123456789012345678901234567890123456789012345678";
  printf("%s\n",string);
  cache_set(cache,key,string,80);

  uint8_t *new = "43";
  cache_set(cache,key,new,strlen(new) + 1);

  uint32_t val_size = 0;

  uint8_t *imstillhere = (uint8_t*) cache_get(cache,standin,&val_size);

  test(imstillhere != NULL,"cache doesnt evict if a value that would cause memory to exceed maxmem is replacing a value in such a way not to exceed maxmem");
  destroy_cache(cache);
}

//cache doesn't remove the old val if the new val is too big for the cache anyways
//creates a cache of size 89, sets 2 values, the second of which has size 80
//inserts a value of size 90 with the same key as the second value inserted
//checks if the value of size 80 (the second one inserted is still there)
//this test is controversial insofar as it is implementation independent whether
//the cache should keep an old value if it cannot fit the new one in the cache
//ideally the cache would store the old val but make note that it is old
//so that it may sue the old in times of high network traffic
//a note will be made in the README
void val_too_big_and_replacing()
{
  cache_t cache = create_cache(89);
  key_type standin = (const uint8_t*) "i shouldnt get removed";
  uint8_t *standinval = "hello there";
  cache_set(cache,standin,standinval,strlen(standinval) + 1);
  key_type key = (const uint8_t*) "doppleganger";
  uint8_t string[80] = "i shall remain";
  cache_set(cache,key,string,80);

  uint8_t new[90] = "01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678";
  cache_set(cache,key,new,90);

  uint32_t val_size = 0;

  uint8_t *notreplaced = (uint8_t*) cache_get(cache,key,&val_size);

  test(!strcmp(notreplaced,"i shall remain"),"cache doesnt replace a value if the new value is too big for the cache");
  destroy_cache(cache);
}

//cache stores its own values instead of references passed in by the user
//sets a value in the cache and then changes the value locally, then checks
//if the value in the cache changed
void cache_mallocing_vals()
{
  cache_t cache = init();
  key_type unique = (const uint8_t*) "we are part of the collective, we have no name";
  uint8_t *mutableval = "34";
  cache_set(cache,unique,mutableval,strlen(mutableval) + 1);

  mutableval = "23";
  uint32_t val_size = 0;
  uint8_t *valnow = (uint8_t*)cache_get(cache,unique,&val_size);
  test(!strcmp(valnow,"34"),"cache malloc's values instead of storing references to (possible) local variables");
  free(valnow);
  destroy_cache(cache);
}

//cache handles large inserts
//inserts a val of size 51
void cache_insert_huge()
{
  cache_t cache = create_cache(512);
  key_type huge = (const uint8_t*) "large";
  uint8_t largeval[51] = "wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww";
  cache_set(cache,huge,largeval,51);
  uint32_t val_size = 0;
  uint8_t *retval = (uint8_t*)cache_get(cache,huge,&val_size);
  test(retval != NULL,"cache handles inserts of huge values");
}

//sets a value in the cache, retrieves that value, changes the value pointed to by the return
//value of cache get, sees if the value in the cache has changed
void cache_returns_bad_pointers()
{
  cache_t cache = init();
  key_type key = (const uint8_t*) "a value";
  uint8_t *num = "25";
  cache_set(cache,key,num,strlen(num) + 1);

  uint32_t val_size = 0;
  uint8_t *ret = (uint8_t*)cache_get(cache,key,&val_size);

  ret[0] = 'n';
  ret[1] = '\0';
  free(ret);
  val_size = 0;
  uint8_t *twentyfive = (uint8_t*)cache_get(cache,key,&val_size);
  test(!strcmp(twentyfive,"25"), "cache returns pointers to copied values, not in cache, when returning from cache_get");
  free(twentyfive);
  destroy_cache(cache);
}

void test_get_head()
{
  cache_t cache = init();
  key_type key = (const uint8_t*) "new";
  uint8_t *value = "123456789";
  cache_set(cache,key,value,strlen(value) + 1);

  get_head(cache);
  destroy_cache(cache);
}

void test_gets()
{
  cache_t cache = init();

  key_type key = "the get key";
  val_type val = "the get value";
  cache_set(cache,key,val,strlen(val) + 1);

  uint32_t val_size = 0;

  /* Get the timebase info */
  mach_timebase_info_data_t info;
  mach_timebase_info(&info);

  uint64_t start = mach_absolute_time();
  for(int i = 0; i < 1000; ++i)
    {
      free(cache_get(cache,key,&val_size));
    }
  uint64_t end = mach_absolute_time();

  uint64_t duration = end - start;

  /* Convert to nanoseconds */
  duration *= info.numer;
  duration /= info.denom;

  duration /= 1000;

  printf("Time per Get: %llu nanoseconds\n",duration);

  destroy_cache(cache);
}

int main(int argc, char *argv[])
{
  hostname = "127.0.0.1";
  tcpport = "2001";
  udpport = "3001";
  for(int i = 2;i < argc; ++i)
    {
      if(!strcmp(argv[i],"-h"))
        hostname = argv[i+1];
      else if (!strcmp(argv[i],"-t"))
        tcpport = argv[i+1];
      else if(!strcmp(argv[i],"-u"))
        udpport = argv[i+1];
    }


  evict_after_get();
  cache_returns_bad_pointers();
  cache_insert_huge();
  test_get_valsize();
  set_multiple();
  //test_empty_size();
  //test_size();
  //test_size_after_delete();
  eviction_couple();
  //struct_set();
  get_modified();
  get_nonexistent();
  resize();
  val_too_big();
  val_too_big_but_replacing();
  cache_mallocing_vals();
  val_too_big_and_replacing();
  //cache_does_not_change_maxmem();
  //custom_hash();
  test_get_head();
  //test_gets(); //udp test
}
