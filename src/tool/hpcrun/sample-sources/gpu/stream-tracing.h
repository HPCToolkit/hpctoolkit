//
// Created by ax4 on 8/5/19.
//

#ifndef HPCTOOLKIT_STREAM_TRACE_THREAD_H
#define HPCTOOLKIT_STREAM_TRACE_THREAD_H

#include <lib/prof-lean/producer_wfq.h>
#include <tool/hpcrun/thread_data.h>
#include <lib/prof-lean/stdatomic.h>
#include <tool/hpcrun/cct/cct.h>
_Atomic(bool) stop_streams;
atomic_ullong stream_counter;

void stream_tracing_init();

void stream_tracing_fini();

void * stream_data_collecting(void* arg);

#endif //HPCTOOLKIT_STREAM_TRACE_THREAD_H
