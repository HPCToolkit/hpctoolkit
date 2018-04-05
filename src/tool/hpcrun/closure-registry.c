//******************************************************************************
// file: closure-registry.c
// purpose: 
//   implementation of support for registering and applying a set of closures
//******************************************************************************



//******************************************************************************
// system includes
//******************************************************************************

#include <stdio.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "closure-registry.h"



//******************************************************************************
// interface functions
//******************************************************************************

void
closure_list_register(
  closure_list_t *l, 
  closure_t *entry
)
{
   entry->next = l->head;
   l->head = entry;
}


int 
closure_list_deregister(
  closure_list_t *l, 
  closure_t *entry
)
{
   closure_t **ptr_to_curr = &l->head;
   closure_t *curr = l->head;
   while (curr != 0) {
     if (curr == entry) {
       *ptr_to_curr = curr->next;
       return 0;
     } 
     ptr_to_curr = &curr->next;
     curr = curr->next;
   }
   return -1;
}


void 
closure_list_apply(
  closure_list_t *l
)
{
   closure_t *entry = l->head;
   while (entry != 0) {
     entry->fn(entry->arg);
     entry = entry->next;
   }
}


void 
closure_list_dump(
  closure_list_t *l
)
{
   printf("closure_list_t * = %p (head = %p)\n", l, l->head);
   closure_t *entry = l->head;
   while (entry != 0) {
     printf("  closure_t* = %p {fn = %p, next = %p}\n", entry, entry->fn, entry->next);
     entry = entry->next;
   }
}

#undef UNIT_TEST_closure_registry
#ifdef UNIT_TEST_closure_registry

void 
c1_fn(
 void *arg
)
{
  printf("in closure 1\n");
}


void 
c2_fn(
 void *arg
)
{
  printf("in closure 2\n");
}


closure_t c1 = {.fn = c1_fn, .next = NULL, .arg = NULL };
closure_t c2 = {.fn = c2_fn, .next = NULL, .arg = NULL };
closure_list_t cl;


int 
main(
  int argc, 
  char **argv)
{
  closure_list_register(&cl, &c1); 
  closure_list_dump(&cl);
  closure_list_apply(&cl);

  closure_list_register(&cl, &c2); 
  closure_list_dump(&cl);
  closure_list_apply(&cl);

  closure_list_deregister(&cl, &c2); 
  closure_list_dump(&cl);
  closure_list_apply(&cl);

  closure_list_deregister(&cl, &c1); 
  closure_list_dump(&cl);
  closure_list_apply(&cl);
}

#endif
