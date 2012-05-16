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

void start_task_fn(void** pointer)
{
  ucontext_t uc;
  getcontext(&uc);
  if(need_defer_cntxt()) {
    omp_arg_t omp_arg;
    omp_arg.tbd = false;
    omp_arg.context = NULL;
    if(TD_GET(region_id) > 0) {
      omp_arg.tbd = true;
      omp_arg.region_id = TD_GET(region_id);
    }
    hpcrun_async_block();
    // record the task creation context into task structure (in omp runtime)
    *pointer = (void *)hpcrun_sample_callpath(&uc, 0, 0, 0, 1, (void*) &omp_arg);
    hpcrun_async_unblock();
  }
  else {
    hpcrun_async_block();
    // record the task creation context into task structure (in omp runtime)
    *pointer = (void *)hpcrun_sample_callpath(&uc, 0, 0, 0, 1, NULL);
    hpcrun_async_unblock();
  }
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
  return hpcrun_cct_insert_path((cct_node_t *)creation_context,
				hpcrun_get_top_cct());
}
