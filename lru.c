//LRU implementation

#include "lru.h"

// adjusts pointers to add a reference to a key-value pair into the LRU queue
void lru_add(evict_class *e, node kvnode, uint64_t htable_index)
{
  if(e->mrupair != NULL)
    {
      kvnode->next = e->mrupair;
      e->mrupair->prev = kvnode;
    }
  else
    {
      kvnode->next = NULL;
      e->lrupair = kvnode;
    }
  e->mrupair = kvnode;
  kvnode->tabindex = htable_index;
}

// Removes node from LRU queue and fixes dangling pointers
// returns the index of the node in the hashtable for deletion by
// the main code
uint64_t lru_remove(evict_class *e)
{
  if(e->lrupair == NULL)
    {
      printf("Value too large to be stored in cache.\n");
      exit(1);
    }

  uint64_t index = e->lrupair->tabindex;
  e->lrupair->tabindex = 0;

  if(e->lrupair->prev != NULL)
    {
      e->lrupair->prev->next = NULL;
      node newlru = e->lrupair->prev;
      e->lrupair->prev = NULL;
      e->lrupair = newlru;
    }
  else
    {
      e->lrupair = NULL;
      e->mrupair = NULL;
    }
  return index;
}
