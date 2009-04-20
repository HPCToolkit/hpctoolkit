// -*-Mode: C++;-*- // technically C99
// $Id: sample_source_itimer.c 2114 2009-04-07 23:29:02Z krentel $

//
// NONE sample source simple oo interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>


/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include "csprof_options.h"
#include "metrics.h"
#include "pmsg.h"
#include "sample_event.h"
#include "sample_source.h"
#include "sample_source_common.h"
#include "sample_sources_registered.h"
#include "simple_oo.h"
#include "thread_data.h"


// ******* METHOD DEFINITIONS ***********

static void
METHOD_FN(init)
{
  self->state = INIT; // no actual init actions necessary for NONE
}

static void
METHOD_FN(_start)
{
  TMSG(NONE_CTL,"starting NONE");

  TD_GET(ss_state)[self->evset_idx] = START;
}

static void
METHOD_FN(stop)
{
  TMSG(NONE_CTL,"stopping NONE");
  TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self,stop); // make sure stop has been called
  self->state = UNINIT;
}

static int
METHOD_FN(supports_event,const char *ev_str)
{
  return (strstr(ev_str,"NONE") != NULL);
}
 
static void
METHOD_FN(process_event_list,int lush_metrics)
{
  TMSG(OPTIONS,"NONE is on the event list");
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
  TMSG(ITIMER_CTL,"No event set for NONE sample source");

  thread_data_t *td = csprof_get_thread_data();
  td->eventSet[self->evset_idx] = 0xDEAD; // Event sets not relevant for itimer
}

/***************************************************************************
 * object
 ***************************************************************************/

sample_source_t _none_obj = {
  // common methods

  .add_event     = csprof_ss_add_event,
  .store_event   = csprof_ss_store_event,
  .get_event_str = csprof_ss_get_event_str,
  .started       = csprof_ss_started,
  .start         = csprof_ss_start,

  // specific methods

  .init = init,
  ._start = _start,
  .stop  = stop,
  .shutdown = shutdown,
  .supports_event = supports_event,
  .process_event_list = process_event_list,
  .gen_event_set = gen_event_set,

  // data
  .evl = {
    .evl_spec = {[0] = '\0'},
    .nevents = 0
  },
  .evset_idx = 2,
  .name = "NONE",
  .state = UNINIT
};


/******************************************************************************
 * constructor 
 *****************************************************************************/

static void none_obj_reg(void) __attribute__ ((constructor));

static void
none_obj_reg(void)
{
  csprof_ss_register(&_none_obj);
}
