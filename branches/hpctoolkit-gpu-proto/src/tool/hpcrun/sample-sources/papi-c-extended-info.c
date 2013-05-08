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
    if (item->pred(name)) return item->sync_setup;
  }
  return no_action;
}

teardown_proc_t
sync_teardown_for_component(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync teardown for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name)) return item->sync_teardown;
  }
  return no_action;
}

start_proc_t
sync_start_for_component(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync start for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name)) return item->sync_start;
  }
  return no_action;
}

stop_proc_t
sync_stop_for_component(int cidx)
{
  const char* name = PAPI_get_component_info(cidx)->name;
  
  TMSG(PAPI, "looking for sync stop for component idx=%d(%s)", cidx, name);
  for(sync_info_list_t* item=registered_sync_components; item; item = item->next) {
    if (item->pred(name)) return item->sync_stop;
  }
  return no_action;
}
