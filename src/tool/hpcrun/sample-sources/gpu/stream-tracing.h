//
// Created by ax4 on 8/5/19.
//

#ifndef HPCTOOLKIT_STREAM_TRACE_THREAD_H
#define HPCTOOLKIT_STREAM_TRACE_THREAD_H

#include "../../../../lib/prof-lean/producer_wfq.h"
#include "../../thread_data.h"
#include "../../cct/cct.h"

#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif

_Atomic(bool) stop_streams;
atomic_ullong stream_counter;

void stream_tracing_init();

void stream_tracing_fini();

void * stream_data_collecting(void* arg);

#endif //HPCTOOLKIT_STREAM_TRACE_THREAD_H
