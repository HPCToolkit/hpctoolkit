//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/cct/cct.h>
#include <hpcrun/module-ignore-map.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>

#include "gpu-activity-channel.h"
#include "gpu-metrics.h"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0

#include "gpu-print.h"



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_application_thread_process_activities
(
 void
)
{
  gpu_activity_channel_consume(gpu_metrics_attribute);
}


cct_node_t *
gpu_application_thread_correlation_callback
(
 uint64_t correlation_id
)
{
  PRINT("enter gpu_correlation_callback %u\n", correlation_id);

  hpcrun_metricVal_t zero_metric_incr = {.i = 0};
  int zero_metric_id = 0; // nothing to see here

  ucontext_t uc;
  getcontext(&uc); // current context, where unwind will begin 

  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter(); 

  cct_node_t *node = 
    hpcrun_sample_callpath(&uc, zero_metric_id,
			   zero_metric_incr, 0, 1, NULL).sample_node;

  hpcrun_safe_exit();

  cct_addr_t *node_addr = hpcrun_cct_addr(node);

  // elide unwanted context from GPU calling context: frames from 
  // libhpcrun and any from GPU load modules registered in the 
  // module ignore map (e.g. libcupti and libcuda, which are stripped)
  static __thread uint16_t libhpcrun_id = 0;

  // the module id of the cct leaf must be libhpcrun, which is not 0.
  // initialize libhpcrun_id once, when it is found to be 0
  if (libhpcrun_id == 0) {
    load_module_t *module = hpcrun_loadmap_findById(node_addr->ip_norm.lm_id);
    if (module != NULL && strstr(module->name, "libhpcrun") != NULL) {
      libhpcrun_id = node_addr->ip_norm.lm_id;
    }
  }


  // skip procedure frames in libhpcrun
  while (libhpcrun_id != 0 && node_addr->ip_norm.lm_id == libhpcrun_id) {
    node = hpcrun_cct_parent(node);
    node_addr = hpcrun_cct_addr(node);
  }

  // skip any procedure frames that should be suppressed,
  // e.g. stripped procedure frames inside libcupti and libcuda
  while (module_ignore_map_module_id_lookup(node_addr->ip_norm.lm_id)) {
    node = hpcrun_cct_parent(node);
    node_addr = hpcrun_cct_addr(node);
  }

  return node;
}
