#include <pthread.h>
#include <stdlib.h>

typedef struct task_t
{
  void* (*job)(void* w);
  void *materials;
  struct task_t *next;
  int id;
} task;

typedef struct work_queue_t
{
  pthread_mutex_t lock;
  task *next_job;
  task *last;
} work_queue;

pthread_t* threadpool_init(int numthreads, work_queue *wq);

void work(work_queue *wq);

void work_queue_add(work_queue *wq, task *task);
