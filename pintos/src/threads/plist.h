#ifndef THREADS_READY_LIST_H
#define THREADS_READY_LIST_H

#include <list.h>
#include "threads/thread.h"

/* A priority_list is an array of lists. Each element in the
   array is a bucket containing a list of elements. The elements
   in each bucket in a list are completely arbitrary, and this is
   an abstraction from the list object. Thus, each bucket is controlled
   via the same method of control used to create lists. */

struct priority_list
  {
    struct list pl_buckets[PRI_MAX + 1];
  };

void plist_init (struct priority_list *pl);
void plist_push_back (struct priority_list *pl, struct list_elem *e, int priority);
void plist_remove (struct list_elem *e);
struct list_elem *plist_pop_front (struct priority_list *pl);
int plist_top_priority (struct priority_list *pl);
bool plist_empty (struct priority_list *pl);

#endif /* threads/ready-list.h */
