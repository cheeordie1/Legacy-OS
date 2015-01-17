#include "threads/plist.h"

void 
plist_init (struct priority_list *pl)
{ 
  ASSERT (pl != NULL);
 
  int curr_b;
  for (curr_b = 0; curr_b <= PRI_MAX; curr_b++)
    list_init (&pl->pl_buckets[curr_b]);
}

/* Adds an element to the ready list. Uses the element's priority to place the
   element in the correct list. */
void
plist_push_back (struct priority_list *pl, struct list_elem *e, int priority)
{
  ASSERT (priority >= PRI_MIN && priority <= PRI_MAX);
  ASSERT (pl != NULL);  

  list_push_back (&pl->pl_buckets[PRI_MAX - priority], e);
}

/* Removes an element from the ready list. */
void
plist_remove (struct list_elem *e)
{
  list_remove (e);
}

/* Pops an element from the the highest priority list that contains an element.
   The caller must also call list_entry to retrieve the overarching object
   that the list_elem is embedded in because that cannot be known in this
   context because it is arbitrary. */
struct list_elem *
plist_pop_front (struct priority_list *pl)
{
  ASSERT (pl != NULL);
 
  int curr_b;
  for (curr_b = 0; curr_b <= PRI_MAX; curr_b++)
    { 
      if (!list_empty (&pl->pl_buckets[curr_b]))
        return list_pop_front (&pl->pl_buckets[curr_b]);
    }
  return NULL;
}

/* Returns a boolean detailing whether the ready list is empty. Iterates
   through the array of ready lists to check whether each list
   is empty. If so returns false, else returns true. */
bool
plist_empty (struct priority_list *pl)
{
  ASSERT (pl != NULL);
  
  int curr_b;
  for (curr_b = 0; curr_b <= PRI_MAX; curr_b++)
    {
      if (!list_empty (&pl->pl_buckets[curr_b]))
        return false;
    }
  return true;
}

/* Return the highest priority of any element in the priority list. */
int
plist_top_priority (struct priority_list *pl)
{
  int curr_b;
  for (curr_b = 0; curr_b <= PRI_MAX; curr_b++)
    {
      if (!list_empty (&pl->pl_buckets[curr_b]))
        return (PRI_MAX - curr_b);
    }
  return PRI_MIN;
}