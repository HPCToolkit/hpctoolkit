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

/*struct kind_info_t {
  int idx;     // current index in kind
  kind_info_t* link; // all kinds linked together in singly linked list
};*/

struct stream_metrics_data_t {
	int n_metrics;
	hpcrun_metricVal_t* null_metrics;
	metric_list_t* metric_data;
	bool has_set_max_metrics;
	metric_list_t* pre_alloc;
	metric_desc_t** id2metric;
	metric_desc_p_tbl_t metric_tbl;
	struct kind_info_t *kinds;
	kind_info_t* current_kind;
	kind_info_t* current_insert;
	metric_upd_proc_t** metric_proc_tbl;
	metric_proc_map_t* proc_map;
};

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
	struct stream_metrics_data_t *metrics;	
	int stream_special_metric_id;

  // ----------------------------------------
  // tracing
  // ----------------------------------------
  uint64_t trace_min_time_us;
  uint64_t trace_max_time_us;
  // ----------------------------------------
  // IO support
  // ----------------------------------------
  FILE* hpcrun_file;

  // ----------------------------------------
  // IDLE NODE persistent id
  // ----------------------------------------
  int32_t idle_node_id;

} stream_data_t;

//---------stream_data.c---------------------
stream_data_t *hpcrun_stream_data_alloc_init(int device_id, int id);
cct_node_t *stream_backtrace2cct(stream_data_t *st, ucontext_t *context);
void hpcrun_stream_finalize(stream_data_t *st);
int hpcrun_generate_stream_backtrace(stream_data_t *st, ucontext_t *context, backtrace_info_t *bt, int skipInner);
extern cct_node_t* hpcrun_cct_insert_backtrace(cct_node_t* cct, frame_t* path_beg, frame_t* path_end);
frame_t* hpcrun_expand_stream_btbuf(stream_data_t *st);
void hpcrun_ensure_stream_btbuf_avail(stream_data_t *st);

//-----------stream_metrics.c------------
kind_info_t* hpcrun_stream_metrics_new_kind();
bool hpcrun_stream_metrics_finalized(stream_data_t *st);
void hpcrun_stream_pre_allocate_metrics(stream_data_t *st, size_t num);
int hpcrun_stream_get_num_metrics(stream_data_t *st);
metric_desc_t* hpcrun_stream_id2metric(stream_data_t *st, int id);
metric_list_t* hpcrun_stream_get_metric_data(stream_data_t *st);
metric_desc_p_tbl_t* hpcrun_stream_get_metric_tbl(stream_data_t *st);
metric_upd_proc_t* hpcrun_stream_get_metric_proc(stream_data_t *st, int metric_id);
int hpcrun_stream_new_metric(stream_data_t *st);
void hpcrun_stream_set_metric_info_w_fn(stream_data_t *st, int metric_id, const char* name,
				 MetricFlags_ValFmt_t valFmt, size_t period,
				 metric_upd_proc_t upd_fn);
void hpcrun_stream_set_metric_info_and_period(stream_data_t *st, int metric_id, const char* name,
				       MetricFlags_ValFmt_t valFmt, size_t period);
void hpcrun_stream_set_metric_info(stream_data_t *st, int metric_id, const char* name);
void hpcrun_stream_et_metric_name(stream_data_t *st, int metric_id, char* name);
metric_set_t* hpcrun_stream_metric_set_new(stream_data_t *st);
cct_metric_data_t* hpcrun_stream_metric_set_loc(metric_set_t* s, int id);
void hpcrun_stream_metric_std_inc(stream_data_t *st, int metric_id, metric_set_t* set,
				  hpcrun_metricVal_t incr);
void hpcrun_stream_metric_set_dense_copy(stream_data_t *st, cct_metric_data_t* dest,
					 metric_set_t* set,
					 int num_metrics);

//---------------write_Stream_data.c-------------

int hpcrun_write_stream_profile_data(stream_data_t *st);

//---------------cct2metrics------------

void stream_cct_metric_data_increment(stream_data_t *st, int metric_id,
			  cct_node_t* x,
			  cct_metric_data_t incr);

metric_set_t*
hpcrun_stream_get_metric_set(stream_data_t *st, cct_node_id_t cct_id);

bool
hpcrun_stream_has_metric_set(stream_data_t *st, cct_node_id_t cct_id);

void stream_cct2metrics_assoc(stream_data_t *st, cct_node_id_t node, metric_set_t* metrics);
#endif























