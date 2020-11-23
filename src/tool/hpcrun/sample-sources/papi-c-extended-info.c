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


void
std_get_event_set(int* ev_s)
{
  int ret = PAPI_create_eventset(ev_s);
  TMSG(PAPI,"PAPI_create_eventset = %d, ev_s = %d", ret, *ev_s);
  if (ret != PAPI_OK) {
    hpcrun_abort("Failure: PAPI_create_eventset.Return code = %d ==> %s", 
                 ret, PAPI_strerror(ret));
  }
}


int
std_add_event(int ev_s, int ev)
{
  return PAPI_add_event(ev_s, ev);
}


int
std_start(int ev_s)
{
  return PAPI_start(ev_s);
}


int
std_read_event(int ev_s, long long *values)
{
  return PAPI_read(ev_s, values);
}


int
std_stop(int ev_s, long long *values)
{
  return PAPI_stop(ev_s, values);
}


get_event_set_proc_t
component_get_event_set(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync get_event_set for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name) && item->get_event_set != NULL) return item->get_event_set;
  }
  return std_get_event_set;
}


add_event_proc_t
component_add_event_proc(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync add_event for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name) && item->add_event != NULL) return item->add_event;
  }
  return std_add_event;
}

finalize_event_set_proc_t
component_finalize_event_set(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync finalize_event_set for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name) && item->finalize_event_set != NULL) return item->finalize_event_set;
  }
  return no_action;
}

bool
component_uses_sync_samples(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "checking component idx %d (name %s) to see if it is synchronous", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name)) {
      TMSG(PAPI, "Component %s IS a synchronous component", name);
      return true;
    }
  }
  return false;
}

setup_proc_t
sync_setup_for_component(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync setup for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name) && item->sync_setup != NULL) return item->sync_setup;
  }
  return no_action;
}

teardown_proc_t
sync_teardown_for_component(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync teardown for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name) && item->sync_teardown != NULL) return item->sync_teardown;
  }
  return no_action;
}

start_proc_t
sync_start_for_component(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync start for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name) && item->start != NULL) return item->start;
  }
  return std_start;
}


read_proc_t
sync_read_for_component(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;

  TMSG(PAPI, "looking for sync start for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name) && item->sync_read != NULL) return item->sync_read;
  }
  return std_read_event;
}


stop_proc_t
sync_stop_for_component(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync stop for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name) && item->stop != NULL) return item->stop;
  }
  return std_stop;
}
