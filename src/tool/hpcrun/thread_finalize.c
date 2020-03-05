//******************************************************************************
// local includes  
//******************************************************************************

#include "thread_finalize.h"



//******************************************************************************
// global variables 
//******************************************************************************

static thread_finalize_entry_t *finalizers;




//******************************************************************************
// interface operations
//******************************************************************************

void
thread_finalize_register(
  thread_finalize_entry_t *e
)
{
  e->next = finalizers;
  finalizers = e;
}


void
thread_finalize
(
  int is_process
)
{
 thread_finalize_entry_t *e = finalizers;
  while (e) {
    e->fn(is_process);
    e = e->next;
  }
}
