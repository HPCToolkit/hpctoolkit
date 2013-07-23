/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <utilities/task-cntxt.h>
#include <utilities/defer-cntxt.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/cct/cct.h>

/******************************************************************************
 * macros
 *****************************************************************************/

/******************************************************************************
 * global variables
 *****************************************************************************/

static int ompt_task_full_ctxt = 0;


/******************************************************************************
 * local functions
 *****************************************************************************/
static
void start_task_fn(ompt_data_t *parent_task_data, 
		   ompt_frame_t *parent_task_frame, ompt_data_t *new_task_data)
{
  void **pointer = &(new_task_data->ptr);
  uint64_t zero_metric_incr = 0LL;

  thread_data_t *td = hpcrun_get_thread_data();
  td->overhead ++;

  ucontext_t uc;
  getcontext(&uc);

  hpcrun_safe_enter();

  // record the task creation context into task structure (in omp runtime)
  *pointer = (void *)
    hpcrun_sample_callpath(&uc, 0, zero_metric_incr, 1, 1).sample_node;

  hpcrun_safe_exit();

  td->overhead --;
}


void task_cntxt_full()
{
  ompt_task_full_ctxt = 1;
}
