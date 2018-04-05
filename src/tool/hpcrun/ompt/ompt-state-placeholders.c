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
#include "../safe-sampling.h"
#include "../hpcrun-initializers.h"



//***************************************************************************
// global variables 
//***************************************************************************

ompt_placeholders_t ompt_placeholders;



//***************************************************************************
// local variables 
//***************************************************************************

static closure_t ompt_placeholder_init_closure;



//***************************************************************************
// private operations
//***************************************************************************

//----------------------------------------------------------------------------
// placeholder functions for blame shift reporting
//----------------------------------------------------------------------------

void 
ompt_idle_state(void)
{
  // this function is a placeholder that represents the calling context of
  // idle OpenMP worker threads. It is not meant to be invoked.
}


void 
ompt_overhead_state(void)
{
  // this function is a placeholder that represents the calling context of
  // threads working in the OpenMP runtime.  It is not meant to be invoked.
}


void 
ompt_barrier_wait_state(void)
{
  // this function is a placeholder that represents the calling context of
  // threads waiting for a barrier in the OpenMP runtime. It is not meant 
  // to be invoked.
}


void 
ompt_task_wait_state(void)
{
  // this function is a placeholder that represents the calling context of
  // threads waiting for a task in the OpenMP runtime. It is not meant 
  // to be invoked.
}


void 
ompt_mutex_wait_state(void)
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
  // protect against receiving a sample here. if we do, we may get 
  // deadlock trying to acquire a lock associated with 
  // fnbounds_enclosing_addr
  hpcrun_safe_enter();
  {
    void *cpc = canonicalize_placeholder(pc);
    p->pc = cpc;
    p->pc_norm = hpcrun_normalize_ip(cpc, pc_to_lm(cpc));
  }
  hpcrun_safe_exit();
}


//***************************************************************************
// interface operations
//***************************************************************************


#define OMPT_PLACEHOLDER_MACRO(f)               \
  {                                             \
    void * fn = (void *) ompt_fn_lookup(#f);    \
    if (!fn) fn = (void *) f ## _state;         \
    init_placeholder(&ompt_placeholders.f, fn); \
  }

void
ompt_init_placeholders_internal(void *arg)
{
  ompt_function_lookup_t ompt_fn_lookup = (ompt_function_lookup_t) arg;
  FOREACH_OMPT_PLACEHOLDER_FN(OMPT_PLACEHOLDER_MACRO)
}


// placeholders can only be initialized after fnbounds module is initialized.
// for this reason, defer placeholder initialization until the end of 
// hpcrun initialization.
void
ompt_init_placeholders(ompt_function_lookup_t ompt_fn_lookup)
{
  // initialize closure for initializer
  ompt_placeholder_init_closure.fn = ompt_init_placeholders_internal; 
  ompt_placeholder_init_closure.arg = ompt_fn_lookup;

  // register closure
  hpcrun_initializers_defer(&ompt_placeholder_init_closure);
}
