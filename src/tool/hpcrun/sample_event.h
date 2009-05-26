#ifndef SAMPLE_EVENT_H
#define SAMPLE_EVENT_H

#include <stdint.h>

#include "csprof_csdata.h"

int csprof_is_initialized(void);

int  hpcrun_async_is_blocked(void); // tallent:FIXME: def in libmonitor_upcalls.c
void csprof_disable_sampling(void);
void csprof_drop_sample(void);

long csprof_num_samples_total(void);
void csprof_inc_samples_blocked_async(void);
void csprof_inc_samples_filtered(void);

void csprof_display_summary(void);

csprof_cct_node_t *
csprof_sample_event(void *context, int metric_id, uint64_t metric_incr, 
		    int is_sync);

#endif /* sample_event_h */
