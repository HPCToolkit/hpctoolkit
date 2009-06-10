#ifndef SAMPLE_EVENT_H
#define SAMPLE_EVENT_H

#include <stdint.h>

#include "csprof_csdata.h"
#include "thread_data.h"


int csprof_is_initialized(void);


//***************************************************************************

// Whenever a thread enters csprof synchronously via a monitor
// callback, don't allow it to reenter asynchronously via a signal
// handler.  Too much of csprof is not signal-handler safe to allow
// this.  For example, printing debug messages could deadlock if the
// signal hits while holding the MSG lock.
//
// This block is only needed per-thread, so the "suspend_sampling"
// thread data is a convenient place to store this.

static inline void
hpcrun_async_block(void)
{
  TD_GET(suspend_sampling) = 1;
}


static inline void
hpcrun_async_unblock(void)
{
  TD_GET(suspend_sampling) = 0;
}


static inline int
hpcrun_async_is_blocked(void)
{
  return ( (! csprof_td_avail()) 
	   || (TD_GET(suspend_sampling) && !ENABLED(ASYNC_RISKY)));
}


//***************************************************************************

void csprof_disable_sampling(void);
void csprof_drop_sample(void);

long csprof_num_samples_total(void);
void csprof_inc_samples_blocked_async(void);
void csprof_inc_samples_filtered(void);

void csprof_display_summary(void);

hpcrun_cct_node_t*
hpcrun_sample_callpath(void *context, int metricId, uint64_t metricIncr, 
		       int skipInner, int isSync);

#endif /* sample_event_h */
