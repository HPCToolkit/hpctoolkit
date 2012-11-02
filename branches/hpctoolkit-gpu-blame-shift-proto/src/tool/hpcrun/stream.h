#ifndef STREAM_H
#define STREAM_H

#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <stdbool.h>
#include <setjmp.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcio-buffer.h>
#include <lib/prof-lean/splay-macros.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lush/lush-pthread.i>
#include <lush/lush-pthread.h>
#include <lush/lush-backtrace.h>
#include <lib/support-lean/OSUtil.h>

#include <memory/mmap.h>
#include <memory/hpcrun-malloc.h>
#include <unwind/common/backtrace.h>
#include "sample_sources_registered.h"
#include "newmem.h"
#include "epoch.h"
#include "cct_insert_backtrace.h"
#include "cct2metrics.h"
#include <cct/cct_addr.h>
#include "metrics.h"
#include "monitor.h"
#include <monitor.h>
#include "handling_sample.h"
#include <memory/hpcrun-malloc.h>
#include <messages/messages.h>
#include <cct/cct.h>
#include "thread_data.h"
#include <trampoline/common/trampoline.h>
#include "fname_max.h"
#include "backtrace.h"
#include "files.h"
#include "rank.h"
#include "cct_bundle.h"
#include "hpcrun_return_codes.h"
#include "loadmap.h"
#include "sample_prob.h"
#include "core_profile_trace_data.h"

extern	int n_metrics;


//---------stream_data.c---------------------
core_profile_trace_data_t *hpcrun_stream_data_alloc_init(int id);
cct_node_t *stream_duplicate_cpu_node(core_profile_trace_data_t *st, ucontext_t *context, cct_node_t *n);
void hpcrun_stream_finalize(core_profile_trace_data_t *st);

//---------------write_Stream_data.c-------------

int hpcrun_write_stream_profile_data(core_profile_trace_data_t *st);

#endif























