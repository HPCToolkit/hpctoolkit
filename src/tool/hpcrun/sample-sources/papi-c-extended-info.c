#include <stdbool.h>
#include <stddef.h>
#include "papi-c-extended-info.h"
#include <papi.h>

#include <messages/messages.h>

static sync_info_list_t* registered_sync_components = NULL;

void
papi_c_sync_register(sync_info_list_t* info)
{
  info->next = registered_sync_components;
  registered_sync_components = info;
}


void
no_action(void)
{
}

const char *
component_get_name(int cidx)
{
  return PAPI_get_component_info(cidx)->name;
}

get_event_set_proc_t
component_get_event_set(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync get_event_set for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name)) return item->get_event_set;
  }
//  hpcrun_abort("Failure: PAPI_create_eventset to not registered component");
  return NULL;
}


add_event_proc_t
component_add_event_proc(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync add_event for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name)) return item->add_event;
  }
//  hpcrun_abort("Failure: PAPI_add_event to not registered component");
  return NULL;
}

finalize_event_set_proc_t
component_finalize_event_set(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync finalize_event_set for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name)) return item->finalize_event_set;
  }
  return no_action;
}

bool
component_uses_sync_samples(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;

  TMSG(PAPI, "checking component idx %d (name %s) to see if it is synchronous", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name)) return item->is_gpu_sync;
  }
  return false;
}

setup_proc_t
sync_setup_for_component(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync setup for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name)) return item->setup;
  }
  return NULL;
}

teardown_proc_t
sync_teardown_for_component(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync teardown for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name)) return item->teardown;
  }
  return NULL;
}

start_proc_t
sync_start_for_component(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync start for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name)) return item->start;
  }
  return NULL;
}


read_proc_t
sync_read_for_component(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;

  TMSG(PAPI, "looking for sync start for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name)) return item->read;
  }
  return NULL;
}


stop_proc_t
sync_stop_for_component(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync stop for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name)) return item->stop;
  }
  return NULL;
}
