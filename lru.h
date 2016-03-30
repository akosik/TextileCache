//LRU Eviction Algorithm Implementation

#include <stdlib.h>
#include <stdio.h>

//Eviction Struct for each key-value pair
//The user needs to redefine this if they want to
//implement their own eviction policy
typedef struct node_t
{
  uint64_t tabindex;
  struct node_t *prev;
  struct node_t *next;
}* node;

struct evict_t;
typedef void (*add_func)(struct evict_t *e, node kvnode,uint64_t htableindex);
typedef uint64_t (*remove_func)(struct evict_t *e);

typedef struct evict_t {
  node lrupair;
  node mrupair;
  add_func add;
  remove_func remove;
} evict_class;

void lru_add(evict_class *e, node kvnode, uint64_t hashtable_index);

uint64_t lru_remove(evict_class *e);
