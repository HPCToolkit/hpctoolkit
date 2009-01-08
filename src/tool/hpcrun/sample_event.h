#ifndef SAMPLE_EVENT_H
#define SAMPLE_EVENT_H

#include "csprof_csdata.h"

extern int samples_taken;
extern int filtered_samples;
extern int bad_unwind_count;

csprof_cct_node_t*
csprof_sample_event(void *context, int metric_id, unsigned long long metric_units_consumed);

void csprof_disable_sampling(void);
void csprof_suspend_sampling(int i);

void csprof_handling_synchronous_sample(int val);
int csprof_handling_synchronous_sample_p();

extern void csprof_drop_sample(void);

#endif /* sample_event_h */
