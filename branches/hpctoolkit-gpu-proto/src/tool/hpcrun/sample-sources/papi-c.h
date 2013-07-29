#ifndef PAPI_C_H
#define PAPI_C_H

#include "papi-c-extended-info.h"

/******************************************************************************
 * type declarations 
 *****************************************************************************/
typedef struct {
  bool inUse;
  int eventSet;
  source_state_t state;
  int some_derived;
  bool scale_by_thread_count;
  long long prev_values[MAX_EVENTS];
  bool is_sync;
  bool setup_process_only;
  get_event_set_proc_t get_event_set;
  add_event_proc_t add_event;
  finalize_event_set_proc_t finalize_event_set;
  start_proc_t sync_start;
  stop_proc_t sync_stop;
  setup_proc_t sync_setup;
  teardown_proc_t sync_teardown;
} papi_component_info_t;

typedef struct {
  int num_components;
  papi_component_info_t component_info[0];
} papi_source_info_t;

extern int get_component_event_set(papi_source_info_t *psi, int cidx);
#endif // PAPI_C_H
