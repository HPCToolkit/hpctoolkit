//***************************************************************************
// local includes 
//***************************************************************************

#include "ompt-state-placeholders.h"



//***************************************************************************
// global variables 
//***************************************************************************

ompt_placeholders_t ompt_placeholders;



//***************************************************************************
// private operations
//***************************************************************************

//----------------------------------------------------------------------------
// placeholder functions for blame shift reporting
//----------------------------------------------------------------------------

void 
omp_idle(void)
{
  // this function is a placeholder used to represent the calling context of
  // idle OpenMP worker threads. It is not meant to be invoked.
  assert(0);
}


void 
omp_overhead(void)
{
  // this function is a placeholder used to represent the OpenMP context of
  // threads working in the OpenMP runtime.  It is not meant to be invoked.
  assert(0);
}


void 
omp_barrier_wait(void)
{
  // this function is a placeholder used to represent the OpenMP context of
  // threads waiting for a barrier in the OpenMP runtime. It is not meant 
  // to be invoked.
  assert(0);
}


void 
omp_task_wait(void)
{
  // this function is a placeholder used to represent the OpenMP context of
  // threads waiting for a task in the OpenMP runtime. It is not meant 
  // to be invoked.
  assert(0);
}


void 
omp_mutex_wait(void)
{
  // this function is a placeholder used to represent the OpenMP context of
  // threads waiting for a mutex in the OpenMP runtime. It is not meant 
  // to be invoked.
  assert(0);
}



//***************************************************************************
// interface operations
//***************************************************************************

#define FINALIZE_PLACEHOLDER(f) \
  if (!ompt_placeholders.f) ompt_placeholders.f = f

#define OMPT_PLACEHOLDER_MACRO(f) \
  ompt_placeholders.f = (f ## _t) ompt_fn_lookup(#f); 

void
ompt_init_placeholder_fn_ptrs(ompt_function_lookup_t ompt_fn_lookup)
{
  // look up placeholders functions available from the runtime
  FOREACH_OMPT_PLACEHOLDER_FN(OMPT_PLACEHOLDER_MACRO)

  // use a tool placeholder function if the runtime doesn't have one 
  FINALIZE_PLACEHOLDER(omp_idle);
  FINALIZE_PLACEHOLDER(omp_overhead);
  FINALIZE_PLACEHOLDER(omp_barrier_wait);
  FINALIZE_PLACEHOLDER(omp_task_wait);
  FINALIZE_PLACEHOLDER(omp_mutex_wait);
}
