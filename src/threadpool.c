#include "threadpool.h"

void work(work_queue *wq)
{
  while(1)
    {
      pthread_mutex_lock(&wq->lock);

      //check if there's a job
      if(wq->next_job != NULL)
	{      
	  //take the job
	  task *myjob = wq->next_job;

	  //remove i from the queue and release the lock
	  wq->next_job = wq->next_job->next;
	  if(wq->last == myjob) wq->last = NULL;
	  pthread_mutex_unlock(&wq->lock);

	  //do the job
	  myjob->job(myjob->materials);
	  free(myjob);
	}
      else	  
	pthread_mutex_unlock(&wq->lock);
    }
}

pthread_t* threadpool_init(int numthreads, work_queue *wq)
{
  pthread_t *threads = calloc(numthreads,sizeof(pthread_t));

  for(int i=0; i < numthreads; ++i)
    {
      pthread_create(&threads[i],NULL,work,wq);
    }
  return threads;
}

void work_queue_add(work_queue *wq, task *task)
{
  pthread_mutex_lock(&wq->lock);

  //add the job to the end
  if(wq->last != NULL) wq->last->next = task;
  wq->last = task;

  //check if it's next
  if(wq->next_job == NULL) wq->next_job = task;

  pthread_mutex_unlock(&wq->lock);
}

/*
void print_queue(work_queue *wq)
{
  for(task *errand = wq->next_job; errand != NULL; errand = errand->next)
    if(errand->id != 34) printf("%d\n",errand);
}
*/
