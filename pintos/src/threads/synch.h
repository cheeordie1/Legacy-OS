#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* A counting semaphore. */
struct semaphore 
  {
    unsigned value;                      /* Current value. */
    struct list waiters;        /* List of waiting threads. */
  };

void sema_init (struct semaphore *, unsigned value);
bool sema_cmp (const struct list_elem *a, 
               const struct list_elem *b, void *aux);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/* Lock. */
struct lock 
  {
    struct thread *holder;            /* Thread holding lock (for debugging). */
    struct semaphore semaphore;       /* Binary semaphore controlling access. */
    struct list_elem lock_list_elem;  /* One lock in a list of acquired locks. */
  };

void lock_init (struct lock *);
void lock_donate (struct lock *lock, int donate_pri);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);

void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* Condition variable. */
struct condition 
  {
    struct list waiters;        /* list of semaphores with waiting threads. */
  };

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

struct shared_lock 
  {
    int i;                 /* Shared lockers, or -1 if exclusively locked. */
    struct lock slock;     /* Lock that people share. */
    struct condition cond; /* Condition variable to share. */
  };

void slock_init (struct shared_lock *sl);
void lock_acquire_exclusive (struct shared_lock *sl);
void lock_acquire_shared (struct shared_lock *sl);
void lock_release_shared (struct shared_lock *sl);
void lock_release_exclusive (struct shared_lock *sl);

/* Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */
