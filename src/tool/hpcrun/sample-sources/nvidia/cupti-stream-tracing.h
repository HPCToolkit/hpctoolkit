//
// Created by ax4 on 8/5/19.
//

#ifndef _HPCTOOLKIT_CUPTI_STREAM_TRACE_THREAD_H_
#define _HPCTOOLKIT_CUPTI_STREAM_TRACE_THREAD_H_

#include <lib/prof-lean/producer_wfq.h>
#include <lib/prof-lean/stdatomic.h>
#include <tool/hpcrun/thread_data.h>
#include <tool/hpcrun/cct/cct.h>

#include "cupti-context-stream-id-map.h"

void cupti_stream_tracing_init();

void cupti_stream_tracing_fini();

void *cupti_stream_data_collecting(void *arg);

void cupti_stream_counter_increase(unsigned long long inc);

#endif //HPCTOOLKIT_STREAM_TRACE_THREAD_H
