/******************************************************************************
 * system includes
 *****************************************************************************/

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <papi.h>
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

#include <omp.h>
#include <dlfcn.h>
#include <hpcrun/loadmap.h>
#include <hpcrun/trace.h>

#include <lib/prof-lean/splay-macros.h>

#include <hpcrun/cct2metrics.h>
#include <hpcrun/metrics.h>
#include <utilities/task-cntxt.h>
#include <utilities/defer-cntxt.h>
#include <hpcrun/unresolved.h>
#include <hpcrun/write_data.h>

#include "/home/xl10/support/gcc-4.6.2/libgomp/libgomp_g.h"

/******************************************************************************
 * macros
 *****************************************************************************/

#define OMPstr "gomp"

/******************************************************************************
 * forward declaration
 *****************************************************************************/

void* need_task_cntxt();
void *copy_task_cntxt(void*);

/******************************************************************************
 * local functions
 *****************************************************************************/

void start_task_fn(void** pointer)
{
  thread_data_t *td = hpcrun_get_thread_data();
  td->overhead ++;
  ucontext_t uc;
  getcontext(&uc);
  void *parent_task_cntxt = NULL;
  if(parent_task_cntxt = need_task_cntxt()) {
    omp_arg_t omp_arg;
    omp_arg.tbd = false;
    omp_arg.region_id = 0;
    // copy the task creation context to local thread
    omp_arg.context = copy_task_cntxt(parent_task_cntxt);

    hpcrun_async_block();
    *pointer = hpcrun_sample_callpath(&uc, 0, 0, 1, 1, (void*) &omp_arg);
    hpcrun_async_unblock();
 
  }
  else if(need_defer_cntxt()) {
    omp_arg_t omp_arg;
    omp_arg.tbd = false;
    omp_arg.context = NULL;
    if(TD_GET(region_id) > 0) {
      omp_arg.tbd = true;
      omp_arg.region_id = TD_GET(region_id);
    }
    hpcrun_async_block();
    // record the task creation context into task structure (in omp runtime)
    *pointer = (void *)hpcrun_sample_callpath(&uc, 0, 0, 1, 1, (void*) &omp_arg);
    hpcrun_async_unblock();
  }
  else {
    hpcrun_async_block();
    // record the task creation context into task structure (in omp runtime)
    *pointer = (void *)hpcrun_sample_callpath(&uc, 0, 0, 1, 1, NULL);
    hpcrun_async_unblock();
  }
  td->overhead --;
}

void register_task_callback()
{
  GOMP_task_callback_register(start_task_fn);
}

void* need_task_cntxt()
{
  if(ENABLED(SET_TASK_CTXT)) {
    void *context = GOMP_get_task_context();
    return context;
  }
  return NULL;
}

void *copy_task_cntxt(void* creation_context)
{
  if(ENABLED(SET_DEFER_CTXT) && (is_partial_resolve((cct_node_t *)creation_context) > 0))
    return hpcrun_cct_insert_path((cct_node_t *)creation_context,
				hpcrun_get_tbd_cct());
  else
    return hpcrun_cct_insert_path((cct_node_t *)creation_context,
				hpcrun_get_top_cct());
}

// It is a hack
// skip all frames above the task frame
// this assumes that there is no parallel regions in the task function
void hack_task_context(frame_t **bt_beg, frame_t **bt_last)
{
  frame_t *bt_p = *bt_beg;
  while(bt_p < *bt_last) {
    load_module_t *lm = hpcrun_loadmap_findById(bt_p->ip_norm.lm_id);
 
    if(strstr(lm->name, OMPstr))
      break;
    bt_p++;
  }
  *bt_last = --bt_p;
}
