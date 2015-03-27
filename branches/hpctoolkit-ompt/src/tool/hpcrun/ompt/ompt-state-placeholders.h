//***************************************************************************
// local include files
//***************************************************************************

#include <ompt.h>



//***************************************************************************
// types
//***************************************************************************

typedef struct {
  omp_idle_t omp_idle;
  omp_overhead_t omp_overhead;
  omp_barrier_wait_t omp_barrier_wait;
  omp_task_wait_t omp_task_wait;
  omp_mutex_wait_t omp_mutex_wait;
} ompt_placeholders_t; 



//***************************************************************************
// forward declarations
//***************************************************************************

extern ompt_placeholders_t ompt_placeholders;



//***************************************************************************
// interface operations 
//***************************************************************************

void
ompt_init_placeholder_fn_ptrs(ompt_function_lookup_t ompt_fn_lookup);

