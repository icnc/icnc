#include "util.h"
#include "queue.h"
#include "config.h"

#ifdef PARALLEL
#include <pthread.h>
#endif //PARALLEL

void queue_init(struct queue * que, int size, int threads) {
#ifdef PARALLEL
  pthread_mutex_init(&que->mutex, NULL);
  pthread_cond_init(&que->empty, NULL);
  pthread_cond_init(&que->full, NULL);
#endif
  que->head = que->tail = 0;
  que->data = (void **)malloc(sizeof(void*) *size);
  que->size = size;
  que->threads = threads;
  que->end_count = 0;
}

void queue_signal_terminate(struct queue * que) {
#ifdef PARALLEL
  pthread_mutex_lock(&que->mutex);
#endif
  que->end_count ++;
#ifdef PARALLEL
  pthread_cond_broadcast(&que->empty);    
  pthread_mutex_unlock(&que->mutex);  
#endif
}

int dequeue(struct queue * que, int * fetch_count, void ** to_buf) {
#ifdef PARALLEL
      pthread_mutex_lock(&que->mutex);
      while (que->tail == que->head && (que->end_count) < que->threads) {
        pthread_cond_wait(&que->empty, &que->mutex);      
      }
#endif
      if (que->tail == que->head && (que->end_count) == que->threads) {
#ifdef PARALLEL
        pthread_cond_broadcast(&que->empty);
        pthread_mutex_unlock(&que->mutex);
#endif
        return -1;     
      }
      for ((*fetch_count) = 0; (*fetch_count) < ITEM_PER_FETCH; (*fetch_count) ++) {
        to_buf[(*fetch_count)] = que->data[que->tail];
        que->tail ++;
        if (que->tail == que->size) que->tail = 0;
        if (que->tail == que->head) {
          (*fetch_count) ++;
          break;
        }
      }
#ifdef PARALLEL
      pthread_cond_signal(&que->full);
      pthread_mutex_unlock(&que->mutex);  
#endif
      return 0;
}

int enqueue(struct queue * que, int * fetch_count, void ** from_buf) {
#ifdef PARALLEL
  pthread_mutex_lock(&que->mutex);
  while (que->head == (que->tail-1+que->size)%que->size)
    pthread_cond_wait(&que->full, &que->mutex);
#endif
  if ((*fetch_count) == -9999) {
    que->data[que->head] = from_buf[0];
    que->head ++;
    if (que->head == que->size) que->head = 0;
  } else {
    while ((*fetch_count) > 0) {
      (*fetch_count) --;
      que->data[que->head] = from_buf[(*fetch_count)];
      que->head ++;
      if (que->head == que->size) que->head = 0;
    }
  }
#ifdef PARALLEL
  pthread_cond_signal(&que->empty);
  pthread_mutex_unlock(&que->mutex);
#endif
  return 0;
}

/*void queue_init(struct queue * que) {
#ifdef PARALLEL
  pthread_mutex_init(&que->mutex, NULL);
  pthread_cond_init(&que->empty, NULL);
  pthread_cond_init(&que->full, NULL);
#endif
  que->head = que->tail = 0;
}

int dequeue(struct queue * que, int * end, int thread_count, int buf_size, int (*copy)(int, int, void*, void *), int * fetch_count, void * from_buf, void * to_buf) {
#ifdef PARALLEL
      pthread_mutex_lock(&que->mutex);
      while (que->tail == que->head && (*end) < thread_count) {
        pthread_cond_wait(&que->empty, &que->mutex);      
      }
#endif
      if (que->tail == que->head && (*end) == thread_count) {
#ifdef PARALLEL
        pthread_cond_broadcast(&que->empty);
        pthread_mutex_unlock(&que->mutex);
#endif
        return -1;     
      }
      for ((*fetch_count) = 0; (*fetch_count) < ITEM_PER_FETCH; (*fetch_count) ++) {
        copy(que->tail, (*fetch_count), from_buf, to_buf);
        que->tail ++;
        if (que->tail == buf_size) que->tail = 0;
        if (que->tail == que->head) {
          (*fetch_count) ++;
          break;
        }
      }
#ifdef PARALLEL
      pthread_cond_signal(&que->full);
      pthread_mutex_unlock(&que->mutex);  
#endif
      return 0;
}

int enqueue(struct queue * que, int buf_size, int (*copy)(int, int, void*, void *), int * fetch_count, void * from_buf, void * to_buf) {
#ifdef PARALLEL
  pthread_mutex_lock(&que->mutex);
  while (que->head == (que->tail-1+buf_size)%buf_size)
    pthread_cond_wait(&que->full, &que->mutex);
#endif
  if ((*fetch_count) == -9999) {
    (*copy)(0, que->head, from_buf, to_buf);
    que->head ++;
    if (que->head == buf_size) que->head = 0;
  } else {
    while ((*fetch_count) > 0) {
      (*fetch_count) --;
      (*copy)((*fetch_count), que->head, from_buf, to_buf);
      que->head ++;
      if (que->head == buf_size) que->head = 0;
    }
  }
#ifdef PARALLEL
  pthread_cond_signal(&que->empty);
  pthread_mutex_unlock(&que->mutex);
#endif
  return 0;
}
*/
