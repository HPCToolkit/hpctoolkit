//******************************************************************************
// system includes
//******************************************************************************

#include <ucontext.h>
#include <dlfcn.h>

#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include <papi.h>
#include <pthread.h>
#include "../libmonitor/monitor.h"



//******************************************************************************
// local includes
//******************************************************************************

#include "../thread_data.h"
#include "../messages/messages.h"
#include "../sample_event.h"
#include "../safe-sampling.h"
#include "../sample_sources_all.h"
#include "../gpu/gpu-activity.h"
#include "common.h"
#include "ss-obj-name.h"

#include "papi-c.h"
#include "papi-c-extended-info.h"



//******************************************************************************
// local data
//******************************************************************************

static __thread bool event_set_created = false;
static __thread bool event_set_finalized = false;
static __thread int my_event_set = PAPI_NULL;



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


void
papi_c_intel_get_event_set(int* event_set)
{
  ETMSG(INTEL, "Started: create intel PAPI event set");
  if (! event_set_created) {
    int ret=PAPI_create_eventset(&my_event_set);
    if (ret!=PAPI_OK) {
      hpcrun_abort("Failure: PAPI_create_eventset.Return code = %d ==> %s",
                   ret, PAPI_strerror(ret));
    }
    *event_set = my_event_set;
    event_set_created = true;
  }
  ETMSG(INTEL, "Completed: create intel PAPI event set");
}


int
papi_c_intel_add_event(int event_set, int evcode)
{
  int rv = PAPI_EMISC;

  if (event_set != my_event_set) return rv;

  if (!event_set_finalized) {
    TMSG(INTEL, "Adding event %x to intel event set", evcode);
    rv = PAPI_add_event(my_event_set, evcode);
    if (rv != PAPI_OK) {
      hpcrun_abort("failure in PAPI gen_event_set(): PAPI_add_event() returned: %s (%d)",
                   PAPI_strerror(rv), rv);
    }
    TMSG(INTEL, "Added event %d, to intel event set %d", evcode, my_event_set);
  }

  return rv;
}


// No adding new events after this point
void
papi_c_intel_finalize_event_set(void)
{
  event_set_finalized = true;
}


void
papi_c_intel_start(void)
{
  ETMSG(INTEL, "Started: start PAPI collection");
  int ret=PAPI_start(my_event_set);
  if (ret!=PAPI_OK) {
    hpcrun_abort("PAPI_start of intel event set %d failed with %s (%d)",
         my_event_set, PAPI_strerror(ret), ret);
  }
  ETMSG(INTEL, "Completed: start PAPI collection");
}


void
papi_c_intel_stop(void)
{
  ETMSG(INTEL, "Started: stop PAPI collection");
  int ret=PAPI_stop(my_event_set, NULL);
  if (ret!=PAPI_OK) {
    hpcrun_abort("PAPI_start of event set %d failed with %s (%d)",
         my_event_set, PAPI_strerror(ret), ret);
  }
  ETMSG(INTEL, "Completed: stop PAPI collection");
}


void
papi_c_intel_read(long long *values)
{
  int ret = PAPI_read(my_event_set, values);
  if (ret != PAPI_OK) {
    hpcrun_abort("PAPI_read of event set %d failed with %s (%d)",
         my_event_set, PAPI_strerror(ret), ret);
  }
}


static sync_info_list_t intel_component = {
  .pred = is_papi_c_intel,
  .get_event_set = papi_c_intel_get_event_set,
  .add_event = papi_c_intel_add_event,
  .finalize_event_set = papi_c_intel_finalize_event_set,
  .sync_setup = papi_c_no_action,
  .sync_teardown = papi_c_no_action,
  .sync_start = papi_c_intel_start,
  .read = papi_c_intel_read,
  .sync_stop = papi_c_intel_stop,
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
