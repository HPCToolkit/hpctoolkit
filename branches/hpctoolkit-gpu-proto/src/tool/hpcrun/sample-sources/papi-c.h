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
  start_proc_t sync_start;
} papi_component_info_t;

typedef struct {
  int num_components;
  papi_component_info_t component_info[0];
} papi_source_info_t;

extern int get_component_event_set(papi_source_info_t *psi, int cidx);
#endif // PAPI_C_H
