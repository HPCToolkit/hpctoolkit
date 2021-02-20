#ifdef ENABLE_OPENCL

//******************************************************************************
// system includes
//******************************************************************************

#include <math.h>															// pow



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/safe-sampling.h>             // hpcrun_safe_enter, hpcrun_safe_exit

#include "../blame.h"                         // sync_prologue, sync_epilogue, etc
#include "opencl-blame.h"
#include "../blame-kernel-cleanup-map.h"      // kernel_cleanup_map_insert



//******************************************************************************
// private operations
//******************************************************************************

static void
releasing_opencl_events
(
 kernel_id_t id_node
)
{
  cl_event ev;
  for (int i = 0; i < id_node.length; i++) {
    kernel_cleanup_map_entry_t *e = kernel_cleanup_map_lookup(id_node.id[i]);
    kernel_cleanup_data_t *d = kernel_cleanup_map_entry_data_get(e);
    ev = d->event;
    clReleaseEvent(ev);
    kcd_free_helper(d);
    kernel_cleanup_map_delete(id_node.id[i]);
  }
}



//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_queue_prologue
(
 cl_command_queue queue
)
{
  queue_prologue((uint64_t) queue);
}


void
opencl_queue_epilogue
(
 cl_command_queue queue
)
{
  queue_epilogue((uint64_t) queue);
}


void
opencl_kernel_prologue
(
 cl_event event
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter(); 

  // increment the reference count for the event
  clRetainEvent(event);
  kernel_cleanup_data_t *data = kcd_alloc_helper();
  data->event = event;
  kernel_cleanup_map_insert((uint64_t) event, data);
  kernel_prologue((uint64_t) event);

  hpcrun_safe_exit();
}


void
opencl_kernel_epilogue
(
 cl_event event
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter(); 

  uint64_t event_id = (uint64_t) event;
  unsigned long kernel_start, kernel_end;
  // clGetEventProfilingInfo returns time in nanoseconds
  cl_int err_cl = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &(kernel_end), NULL);

  if (err_cl == CL_SUCCESS) {
    float elapsedTime;
    err_cl = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &(kernel_start), NULL);

    if (err_cl != CL_SUCCESS) {
      EMSG("clGetEventProfilingInfo failed");
    }

    // Just to verify that this is a valid profiling value. 
    elapsedTime = kernel_end- kernel_start;
    if (elapsedTime <= 0) {
      printf("bad kernel time\n");
      hpcrun_safe_exit();
      return;
    }
		kernel_epilogue(event_id, kernel_start, kernel_end);

  } else {
    EMSG("clGetEventProfilingInfo failed");
  }

  hpcrun_safe_exit();
}


void
opencl_sync_prologue
(
 cl_command_queue queue
)
{
  sync_prologue((uint64_t)queue);
}


void
opencl_sync_epilogue
(
 cl_command_queue queue
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter(); 

  // we cant release opencl events at kernel_epilogue. Their unique event ids can be used later for attributing cpu_idle_blame.
  // so we release them once they are processed in sync_epilogue
  kernel_id_t id_node = sync_epilogue((uint64_t)queue);
  releasing_opencl_events(id_node);

  hpcrun_safe_exit();
}

#endif	// ENABLE_OPENCL
