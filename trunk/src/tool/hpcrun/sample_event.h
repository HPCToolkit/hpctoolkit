#ifndef SAMPLE_EVENT_H
#define SAMPLE_EVENT_H

#include "csprof_csdata.h"

extern int samples_taken;
extern int filtered_samples;
extern int bad_unwind_count;

int csprof_is_initialized(void);

int  csprof_async_is_blocked(void);
void csprof_disable_sampling(void);
void csprof_drop_sample(void);

void csprof_inc_samples_blocked_async(void);
long csprof_num_samples_blocked_async(void);

csprof_cct_node_t *
csprof_sample_event(void *context, int metric_id,
		    unsigned long long metric_units_consumed);

#endif /* sample_event_h */
