//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>
#include <pthread.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/placeholders.h>
#include <hpcrun/fnbounds/fnbounds_interface.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/hpcrun-initializers.h>

#include "hpcrun-placeholders.h"



//******************************************************************************
// private variables 
//******************************************************************************

static placeholder_t hpcrun_placeholders[hpcrun_placeholder_type_count];

static pthread_once_t is_initialized = PTHREAD_ONCE_INIT;



//******************************************************************************
// private operations
//******************************************************************************

void
hpcrun_no_activity
(
 void
)
{
  // this function is not meant to be called
  assert(0);
}


static void
hpcrun_default_placeholders_init
(
 void
)
{
  init_placeholder(&hpcrun_placeholders[hpcrun_placeholder_type_no_activity], 
		   hpcrun_no_activity);
}



//******************************************************************************
// interface operations
//******************************************************************************

load_module_t *
pc_to_lm
(
    void *pc
)
{
    void *func_start_pc, *func_end_pc;
    load_module_t *lm = NULL;
    fnbounds_enclosing_addr(pc, &func_start_pc, &func_end_pc, &lm);
    return lm;
}


void
init_placeholder
(
     placeholder_t *p,
     void *pc
)
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


placeholder_t *
hpcrun_placeholder_get
(
 hpcrun_placeholder_type_t ph_type
)
{
  pthread_once(&is_initialized, hpcrun_default_placeholders_init);

  return &hpcrun_placeholders[ph_type];
}
