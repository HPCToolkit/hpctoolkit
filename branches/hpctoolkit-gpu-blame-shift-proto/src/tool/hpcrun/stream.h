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


extern	int n_metrics;

typedef struct stream_data_t {
  int device_id;
  int id;
  // ----------------------------------------
  // epoch: loadmap + cct + cct_ctxt
  // ----------------------------------------
  epoch_t* epoch;
	frame_t* btbuf_cur;  // current frame when actively constructing a backtrace
  frame_t* btbuf_beg;
	frame_t* btbuf_end;  
	frame_t* btbuf_sav;
	backtrace_t bt; 

	//metrics: this is needed otherwise 
	//hpcprof does not pick them up
	cct2metrics_t* cct2metrics_map;

  // ----------------------------------------
  // tracing
  // ----------------------------------------
  uint64_t trace_min_time_us;
  uint64_t trace_max_time_us;
  // ----------------------------------------
  // IO support
  // ----------------------------------------
  FILE* hpcrun_file;

} stream_data_t;

//---------stream_data.c---------------------
stream_data_t *hpcrun_stream_data_alloc_init(int device_id, int id);
cct_node_t *stream_duplicate_cpu_node(stream_data_t *st, ucontext_t *context, cct_node_t *n);
void hpcrun_stream_finalize(stream_data_t *st);
int hpcrun_generate_stream_backtrace(stream_data_t *st, ucontext_t *context, backtrace_info_t *bt, int skipInner);
extern cct_node_t* hpcrun_cct_insert_backtrace(cct_node_t* cct, frame_t* path_beg, frame_t* path_end);
frame_t* hpcrun_expand_stream_btbuf(stream_data_t *st);
void hpcrun_ensure_stream_btbuf_avail(stream_data_t *st);

//---------------write_Stream_data.c-------------

int hpcrun_write_stream_profile_data(stream_data_t *st);

#endif























