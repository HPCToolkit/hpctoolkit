#ifndef sample_event_h
#define sample_event_h

#include "csprof_csdata.h"

csprof_cct_node_t*
csprof_sample_event(void *context, int metric_id, size_t sample_count);

int csprof_is_handling_sample(void);

#endif /* sample_event_h */
