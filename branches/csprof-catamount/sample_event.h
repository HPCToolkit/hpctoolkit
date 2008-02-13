// -*-Mode: C++;-*- // technically C99
// $Id$

#ifndef sample_event
#define sample_event

//************************* System Include Files ****************************

#include <ucontext.h>

//*************************** User Include Files ****************************

#include "metrics_types.h"

//*************************** Forward Declarations **************************

void 
csprof_sample_event(ucontext_t* context, int metric_id, size_t sample_count);

#endif /* sample_event */
