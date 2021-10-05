//******************************************************************************
// system includes
//******************************************************************************

#include <ucontext.h> 
#include <dlfcn.h>

#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include <papi.h>
#include <monitor.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/spinlock.h>
#include <hpcrun/thread_data.h>
#include <messages/messages.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_sources_all.h>
#include <hpcrun/gpu/gpu-activity.h>
#include <sample-sources/common.h>
#include <sample-sources/ss-obj-name.h>

#include "papi-c.h"
#include "papi-c-extended-info.h"



//******************************************************************************
// local data
//******************************************************************************

#define numMetrics 3
#define ACTIVE_INDEX 1
#define STALL_INDEX 1
#define MAX_STR_LEN     128
static spinlock_t setup_lock = SPINLOCK_UNLOCKED;
int eventset;

char const *metric_name[MAX_STR_LEN] = {
           "ComputeBasic.GpuTime",
           "ComputeBasic.EuActive",
           "ComputeBasic.EuStall"
};



//******************************************************************************
// private operations
//******************************************************************************

static void
papi_c_no_action(void)
{
  ;
}


static bool
is_papi_c_intel(const char* name)
{
  return strstr(name, "intel") == name;
}


static void
papi_c_intel_setup(void)
{
  TMSG(INTEL, "Started: setup intel PAPI");
  spinlock_lock(&setup_lock);
  eventset = PAPI_NULL;
  int retval=PAPI_library_init(PAPI_VER_CURRENT);
  if (retval!=PAPI_VER_CURRENT) {
    fprintf(stderr,"Error initializing PAPI! %s\n", PAPI_strerror(retval));
    pthread_exit(NULL);
  }
  spinlock_unlock(&setup_lock);
  TMSG(INTEL, "Completed: setup intel PAPI");
}


void
papi_c_intel_get_event_set(int* ev_s)
{
  TMSG(INTEL, "Started: create intel PAPI event set");
  spinlock_lock(&setup_lock);
  int retval=PAPI_create_eventset(&eventset);
  if (retval!=PAPI_OK) {
    fprintf(stderr,"Error creating eventset! %s\n", PAPI_strerror(retval));
  }
  spinlock_unlock(&setup_lock);
  TMSG(INTEL, "Completed: create intel PAPI event set");
}


int
papi_c_intel_add_event(int ev_s, int ev)
{
  int rv = PAPI_OK;
  int retval;
  TMSG(INTEL, "Started: add event to intel PAPI event set");
  spinlock_lock(&setup_lock);
  for (int i=0; i<numMetrics; i++) {
    retval=PAPI_add_named_event(eventset, metric_name[i]);
    if (retval!=PAPI_OK) {
      fprintf(stderr,"Error adding %s: %s\n", metric_name[i], PAPI_strerror(retval));
    }
  }
  spinlock_unlock(&setup_lock);
  TMSG(INTEL, "Completed: add event to intel PAPI event set");
  return rv;
}


void
papi_c_intel_finalize_event_set(void)
{
  TMSG(INTEL, "Started: finalize PAPI event set");
  spinlock_lock(&setup_lock);
  PAPI_reset(eventset);
  int retval=PAPI_start(eventset);
  if (retval!=PAPI_OK) {
    fprintf(stderr,"Error starting papi collection: %s\n", PAPI_strerror(retval));
  }
  spinlock_unlock(&setup_lock);
  TMSG(INTEL, "Completed: finalize PAPI event set");
}


static void
papi_c_intel_teardown(void)
{
  TMSG(INTEL, "Started: teardown of intel PAPI datastructures");
  spinlock_lock(&setup_lock);
  // free(metric_values);
  PAPI_cleanup_eventset(eventset);
  PAPI_destroy_eventset(&eventset);
  spinlock_unlock(&setup_lock);
  TMSG(INTEL, "Completed: teardown of intel PAPI datastructures");
}


static void
attribute_gpu_utilization(cct_node_t *cct_node, long long *current_values, long long *previous_values)
{
  gpu_activity_t ga;
  gpu_activity_t *ga_ptr = &ga;
  ga_ptr->kind = GPU_ACTIVITY_INTEL_GPU_UTILIZATION;
  ga_ptr->cct_node = cct_node;

  ga_ptr->details.gpu_utilization_info.active = current_values[ACTIVE_INDEX];
  ga_ptr->details.gpu_utilization_info.stalled = current_values[STALL_INDEX];
  ga_ptr->details.gpu_utilization_info.idle = 100 - (current_values[ACTIVE_INDEX] + current_values[STALL_INDEX]);
  gpu_metrics_attribute(ga_ptr);
}


static long long *
papi_c_intel_read(cct_node_t **cct_nodes, uint32_t num_ccts, long long *previous_values)
{
  long long *metric_values = (long long *)hpcrun_malloc(numMetrics * sizeof(long long));
  int retval = PAPI_read(eventset, metric_values);
  if (retval!=PAPI_OK) {
    TMSG(INTEL, "Error stopping:  %s\n", PAPI_strerror(retval));
    return NULL;
  }
  for(int i=0; i<num_ccts; i++) {
    attribute_gpu_utilization(cct_nodes[i], metric_values, previous_values);
  }
  return metric_values;
}


static sync_info_list_t intel_component = {
  .pred = is_papi_c_intel,
  .get_event_set = papi_c_intel_get_event_set,
  .add_event = papi_c_intel_add_event,
  .finalize_event_set = papi_c_intel_finalize_event_set,
  .sync_setup = papi_c_intel_setup,
  .sync_teardown = papi_c_intel_teardown,
  .sync_start = papi_c_no_action,
  .read = papi_c_intel_read,
  .sync_stop = papi_c_no_action,
  .process_only = true,
  .next = NULL,
};



//******************************************************************************
// interface operations
//******************************************************************************

void
SS_OBJ_CONSTRUCTOR(papi_c_intel)(void)
{
  papi_c_sync_register(&intel_component);
}
