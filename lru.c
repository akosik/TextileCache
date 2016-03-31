//LRU implementation

#include "lru.h"

// adjusts pointers to add a reference to a key-value pair into the LRU queue
void lru_add(evict_class *e, node kvnode, uint64_t htable_index)
{
  //already the mrupair? don't do anything
  if(e->mrupair != kvnode)
    {
      //if you aren't the mrupair but are the lrupair, there must be other nodes
      if(e->lrupair == kvnode)
        {
          e->lrupair = kvnode->prev;
          kvnode->prev->next = NULL;
          kvnode->next = e->mrupair;
          kvnode->next->prev = kvnode;
        }
      // middle of the queue
      else if(kvnode->next != NULL && kvnode->prev != NULL)
        {
          kvnode->next->prev = kvnode->prev;
          kvnode->prev->next = kvnode->next;
          kvnode->next = e->mrupair;
          kvnode->next->prev = kvnode;
        }
      //null queue
      else if(e->mrupair == NULL && e->lrupair == NULL)
        {
          e->lrupair = kvnode;
        }
      //aren't in the queue yet
      else
        {
          kvnode->next = e->mrupair;
          kvnode->next->prev = kvnode;
        }
      //necessary updates if you aren't the mru already
      kvnode->prev = NULL;
      e->mrupair = kvnode;
      kvnode->tabindex = htable_index;
    }
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
  //null queue
  else
    {
      e->lrupair = NULL;
      e->mrupair = NULL;
    }
  return index;
}
