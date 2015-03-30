//***************************************************************************
// system includes 
//***************************************************************************

#include <assert.h>



//***************************************************************************
// local includes 
//***************************************************************************

#include "ompt-state-placeholders.h"
#include "../fnbounds/fnbounds_interface.h"
#include "../../../lib/prof-lean/placeholders.h"



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
  // this function is a placeholder that represents the calling context of
  // idle OpenMP worker threads. It is not meant to be invoked.
}


void 
omp_overhead(void)
{
  // this function is a placeholder that represents the calling context of
  // threads working in the OpenMP runtime.  It is not meant to be invoked.
}


void 
omp_barrier_wait(void)
{
  // this function is a placeholder that represents the calling context of
  // threads waiting for a barrier in the OpenMP runtime. It is not meant 
  // to be invoked.
}


void 
omp_task_wait(void)
{
  // this function is a placeholder that represents the calling context of
  // threads waiting for a task in the OpenMP runtime. It is not meant 
  // to be invoked.
}


void 
omp_mutex_wait(void)
{
  // this function is a placeholder that represents the calling context of
  // threads waiting for a mutex in the OpenMP runtime. It is not meant 
  // to be invoked.
}


//***************************************************************************
// private operations
//***************************************************************************

static load_module_t *
pc_to_lm(void *pc)
{
  void *func_start_pc, *func_end_pc;
  load_module_t *lm = NULL;
  fnbounds_enclosing_addr(pc, &func_start_pc, &func_end_pc, &lm);
  return lm;
}


static void 
init_placeholder(ompt_placeholder_t *p, void *pc)
{
  void *cpc = canonicalize_placeholder(pc);
  p->pc = cpc;
  p->pc_norm = hpcrun_normalize_ip(cpc, pc_to_lm(cpc));
}


//***************************************************************************
// interface operations
//***************************************************************************


#define OMPT_PLACEHOLDER_MACRO(f)               \
  {                                             \
    f ## _t fn = (f ## _t) ompt_fn_lookup(#f);  \
    if (!fn) fn = f;                            \
    init_placeholder(&ompt_placeholders.f, fn); \
  }

void
ompt_init_placeholders(ompt_function_lookup_t ompt_fn_lookup)
{
  FOREACH_OMPT_PLACEHOLDER_FN(OMPT_PLACEHOLDER_MACRO)
}
