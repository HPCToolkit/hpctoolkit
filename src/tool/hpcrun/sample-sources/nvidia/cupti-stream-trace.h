//
// Created by ax4 on 8/5/19.
//

#ifndef _HPCTOOLKIT_CUPTI_STREAM_TRACE_H_
#define _HPCTOOLKIT_CUPTI_STREAM_TRACE_H_

#include <lib/prof-lean/producer_wfq.h>
#include <lib/prof-lean/stdatomic.h>
#include <tool/hpcrun/thread_data.h>
#include <tool/hpcrun/cct/cct.h>

typedef struct stream_trace_s stream_trace_t;

void cupti_stream_trace_init();

void cupti_stream_trace_fini(void *arg);

void *cupti_stream_trace_collect(void *arg);

stream_trace_t *cupti_stream_trace_create();

void cupti_stream_trace_append(stream_trace_t *stream_trace, uint64_t start, uint64_t end, cct_node_t *cct_node);

#endif // _HPCTOOLKIT_STREAM_TRACE_H_
