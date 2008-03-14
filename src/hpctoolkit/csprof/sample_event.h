#ifndef SAMPLE_EVENT_H
#define SAMPLE_EVENT_H

#include "csprof_csdata.h"

extern int samples_taken;
extern int filtered_samples;
extern int bad_unwind_count;

csprof_cct_node_t*
csprof_sample_event(void *context, int metric_id, size_t sample_count);

#endif /* sample_event_h */
