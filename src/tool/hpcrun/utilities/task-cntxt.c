/******************************************************************************
 * system includes
 *****************************************************************************/

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <stdbool.h>

#include <pthread.h>

/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/cct/cct.h>
#include <messages/messages.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/atomic.h>

#include <dlfcn.h>
#include <hpcrun/loadmap.h>
#include <hpcrun/trace.h>

#include <lib/prof-lean/splay-macros.h>

#include <hpcrun/safe-sampling.h>
#include <hpcrun/cct2metrics.h>
#include <hpcrun/metrics.h>
#include <utilities/task-cntxt.h>
#include <utilities/defer-cntxt.h>
#include <hpcrun/unresolved.h>
#include <hpcrun/write_data.h>

#include "hpcrun/ompt.h"

/******************************************************************************
 * macros
 *****************************************************************************/

/******************************************************************************
 * global variables
 *****************************************************************************/

static int ompt_task_full_ctxt = 0;

/******************************************************************************
 * forward declaration
 *****************************************************************************/

void* need_task_cntxt();
void *copy_task_cntxt(void*);

/******************************************************************************
 * local functions
 *****************************************************************************/

void start_task_fn(ompt_data_t *parent_task_data, ompt_frame_t *parent_task_frame, ompt_data_t *new_task_data)
{
  void **pointer = &(new_task_data->ptr);
  uint64_t zero_metric_incr = 0LL;
  thread_data_t *td = hpcrun_get_thread_data();
  td->overhead ++;
  ucontext_t uc;
  getcontext(&uc);
  void *parent_task_cntxt = NULL;
  if ((parent_task_cntxt = need_task_cntxt())) {
    omp_arg_t omp_arg;
    omp_arg.tbd = false;
    omp_arg.region_id = 0;
    // copy the task creation context to local thread
    omp_arg.context = copy_task_cntxt(parent_task_cntxt);

    hpcrun_safe_enter();
    *pointer = hpcrun_sample_callpath(&uc, 0, zero_metric_incr, 1, 1, (void*) &omp_arg).sample_node;
    hpcrun_safe_exit();
 
  }
  else if(need_defer_cntxt()) {
    omp_arg_t omp_arg;
    omp_arg.tbd = false;
    omp_arg.context = NULL;
    // start_task_fn is always called in a parallel region
    // after calling resolve_cntxt(), td->region_id is the region ID of
    // the outer most region in the current thread

    // becaue tasks are always inside a parallel region, we can wait here 
    // if the parallel region id is not set
    while(!TD_GET(region_id))
      resolve_cntxt();
    omp_arg.tbd = true;
    omp_arg.region_id = TD_GET(region_id);
    hpcrun_safe_enter();
    // record the task creation context into task structure (in omp runtime)
    *pointer = (void *)hpcrun_sample_callpath(&uc, 0, zero_metric_incr, 1, 1, (void*) &omp_arg).sample_node;
    hpcrun_safe_exit();
  }
  else {
    hpcrun_safe_enter();
    // record the task creation context into task structure (in omp runtime)
    *pointer = (void *)hpcrun_sample_callpath(&uc, 0, zero_metric_incr, 1, 1, NULL).sample_node;
    hpcrun_safe_exit();
  }
  td->overhead --;
}

void task_cntxt_full()
{
  ompt_task_full_ctxt = 1;
}


void* need_task_cntxt()
{
  if(ompt_task_full_ctxt && ompt_get_task_data(0)) {
    void *context = ompt_get_task_data(0)->ptr;
    return context;
  }
  return NULL;
}

void *copy_task_cntxt(void* creation_context)
{
  if((is_partial_resolve((cct_node_t *)creation_context) > 0))
    return hpcrun_cct_insert_path_return_leaf
      ((hpcrun_get_thread_epoch()->csdata).unresolved_root, 
       (cct_node_t *)creation_context);
  else
    return hpcrun_cct_insert_path_return_leaf
      ((hpcrun_get_thread_epoch()->csdata).tree_root, 
       (cct_node_t *)creation_context);
}

// It is a hack
// skip all frames above the task frame
// this assumes that there is no parallel regions in the task function
void hack_task_context(frame_t **bt_beg, frame_t **bt_last)
{
#if 0
  frame_t *bt_p = *bt_beg;
  while(bt_p < *bt_last) {
    load_module_t *lm = hpcrun_loadmap_findById(bt_p->ip_norm.lm_id);
 
    if(strstr(lm->name, OMPstr))
      break;
    bt_p++;
  }
  *bt_last = --bt_p;
#endif
}
