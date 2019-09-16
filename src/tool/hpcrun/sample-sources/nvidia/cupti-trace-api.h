//
// Created by ax4 on 8/5/19.
//

#ifndef _HPCTOOLKIT_CUPTI_TRACE_API_H_
#define _HPCTOOLKIT_CUPTI_TRACE_API_H_

#include <lib/prof-lean/producer_wfq.h>
#include <lib/prof-lean/stdatomic.h>
#include <tool/hpcrun/thread_data.h>
#include <tool/hpcrun/cct/cct.h>

#include "cupti-node.h"
#include "cupti-channel.h"

typedef struct cupti_trace_s cupti_trace_t;

typedef void (*cupti_trace_fn_t)(cupti_trace_t *trace, void *arg);

void cupti_trace_init();

void cupti_trace_fini(void *arg);

void *cupti_trace_collect(void *arg);

cupti_trace_t *cupti_trace_create();

void cupti_trace_handle(cupti_entry_trace_t *entry);

void cupti_trace_append(cupti_trace_t *trace, void *arg);

#endif // _HPCTOOLKIT_CUPTI_TRACE_API_H_
