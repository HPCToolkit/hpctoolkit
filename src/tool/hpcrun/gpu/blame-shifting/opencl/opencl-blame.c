#ifdef ENABLE_OPENCL

//******************************************************************************
// system includes
//******************************************************************************

#include <math.h>                                                                                                                       // pow



//******************************************************************************
// local includes
//******************************************************************************

#include "../../../safe-sampling.h"             // hpcrun_safe_enter, hpcrun_safe_exit

#include "../blame.h"                         // sync_prologue, sync_epilogue, etc
#include "../../opencl/opencl-api.h"     // place_cct_under_opencl_kernel
#include "opencl-blame.h"
#include "../blame-kernel-cleanup-map.h"      // kernel_cleanup_map_insert
#include "../../gpu-activity-channel.h"                        // gpu_activity_channel_get
#include "../../opencl/intel/papi/papi-metric-collector.h"     // add_kernel_to_incomplete_list, remove_kernel_from_incomplete_list



//******************************************************************************
// private operations
//******************************************************************************

static void
releasing_opencl_events
(
  typeof(&clReleaseEvent) pfn_clReleaseEvent,
  kernel_id_t id_node
)
{
  cl_event ev;
  for (int i = 0; i < id_node.length; i++) {
    kernel_cleanup_map_entry_t *e = kernel_cleanup_map_lookup(id_node.id[i]);
    kernel_cleanup_data_t *d = kernel_cleanup_map_entry_data_get(e);
    ev = d->event;
    pfn_clReleaseEvent(ev);
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
  typeof(&clRetainEvent) pfn_clRetainEvent,
  cl_event event,
  uint32_t kernel_module_id
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter();

  uint64_t event_id = (uint64_t) event;

  // increment the reference count for the event
  pfn_clRetainEvent(event);
  kernel_cleanup_data_t *data = kcd_alloc_helper();
  data->event = event;
  kernel_cleanup_map_insert(event_id, data);
  cct_node_t *cct = place_cct_under_opencl_kernel(kernel_module_id);
  kernel_prologue(event_id, cct);
  if (get_gpu_utilization_flag()) {
    papi_metric_collection_at_kernel_start(event_id, cct, gpu_activity_channel_get());
  }

  hpcrun_safe_exit();
}


void
opencl_kernel_epilogue
(
  typeof(&clGetEventProfilingInfo) pfn_clGetEventProfilingInfo,
  cl_event event
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter();

  uint64_t event_id = (uint64_t) event;
  unsigned long kernel_start, kernel_end;
  // clGetEventProfilingInfo returns time in nanoseconds
  cl_int err_cl = pfn_clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &kernel_end, NULL);

  if (err_cl == CL_SUCCESS) {
    float elapsedTime;
    err_cl = pfn_clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &kernel_start, NULL);

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
    if (get_gpu_utilization_flag()) {
      papi_metric_collection_at_kernel_end(event_id);
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
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter();

  struct timespec sync_start;
  // using CLOCK_MONOTONIC_RAW, CLOCK_MONOTONIC gives time that matches with kernel start and end time outputted by clGetEventProfilingInfo
  // CLOCK_REALTIME gives a timestamp that isn't comparable with kernel start and end time
  clock_gettime(CLOCK_MONOTONIC_RAW, &sync_start); // get current time
  sync_prologue((uint64_t)queue, sync_start);

  hpcrun_safe_exit();
}


void
opencl_sync_epilogue
(
  typeof(&clReleaseEvent) pfn_clReleaseEvent,
  cl_command_queue queue,
  uint16_t num_sync_events
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter();

  struct timespec sync_end;
  // using CLOCK_MONOTONIC_RAW, CLOCK_MONOTONIC gives time that matches with kernel start and end time outputted by clGetEventProfilingInfo
  // CLOCK_REALTIME gives a timestamp that isn't comparable with kernel start and end time
  clock_gettime(CLOCK_MONOTONIC_RAW, &sync_end); // get current time

  // we can't release opencl events at kernel_epilogue. Their unique event ids can be used later for attributing cpu_idle_blame.
  // so we release them once they are processed in sync_epilogue
  kernel_id_t id_node = sync_epilogue((uint64_t)queue, sync_end, num_sync_events);
  releasing_opencl_events(pfn_clReleaseEvent, id_node);

  hpcrun_safe_exit();
}

#endif  // ENABLE_OPENCL
