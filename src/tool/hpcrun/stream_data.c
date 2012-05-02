// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-gpu-blame-shift-proto/src/tool/hpcrun/stream_data.c $
// $Id: stream_data.c 3672 2012-02-21 00:39:22Z mfagan $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2011, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//
//

//************************* System Include Files ****************************

//************************ libmonitor Include Files *************************

//*************************** User Include Files ****************************

#include "stream.h"

//***************************************************************************

/*struct kind_info_t {
  int idx;     // current index in kind
  struct kind_info_t* link; // all kinds linked together in singly linked list
}kind_info_t;
*/

stream_data_t *hpcrun_stream_data_alloc_init(int device_id, int id)
{
	stream_data_t *st = hpcrun_mmap_anon(sizeof(stream_data_t));
  // FIXME: revisit to perform this memstore operation appropriately.
  //memstore = td->memstore;
  memset(st, 0xfe, sizeof(stream_data_t));
  //td->memstore = memstore;
  //hpcrun_make_memstore(&td->memstore, is_child);

	st->device_id = device_id;
  st->id = id;
  st->epoch = hpcrun_malloc(sizeof(epoch_t));
	st->epoch->csdata_ctxt = copy_thr_ctxt(TD_GET(epoch)->csdata.ctxt); //copy_thr_ctxt(thr_ctxt);
  hpcrun_cct_bundle_init(&(st->epoch->csdata), (st->epoch->csdata).ctxt);
 // hpcrun_cct_bundle_init(&(st->epoch->csdata), NULL);
  st->epoch->loadmap = hpcrun_getLoadmap();
  st->epoch->next  = NULL;

	hpcrun_cct2metrics_init(&(st->cct2metrics_map)); //this just does st->map = NULL;

	st->metrics = (struct stream_metrics_data_t *)hpcrun_malloc(sizeof(struct stream_metrics_data_t));

	st->metrics->n_metrics = 0;
	st->metrics->metric_data = NULL;
	st->metrics->has_set_max_metrics = false;
	st->metrics->pre_alloc = NULL;
	st->metrics->kinds = (kind_info_t *)hpcrun_malloc(sizeof(kind_info_t));
	st->metrics->kinds->idx =0; st->metrics->kinds->link = NULL;
	st->metrics->current_kind = st->metrics->kinds;
	st->metrics->current_insert = st->metrics->kinds;
	st->metrics->metric_proc_tbl = NULL;
	st->metrics->proc_map = NULL;


  st->btbuf_cur = NULL;
  st->btbuf_beg = hpcrun_malloc(sizeof(frame_t) * BACKTRACE_INIT_SZ);
  st->btbuf_end = st->btbuf_beg + BACKTRACE_INIT_SZ;
  st->btbuf_sav = st->btbuf_end;  // FIXME: is this needed?

  hpcrun_bt_init(&(st->bt), NEW_BACKTRACE_INIT_SZ);


  st->trace_min_time_us = 0;
  st->trace_max_time_us = 0;
  st->hpcrun_file  = NULL;

	/*FIXME: need to look at golden number later*/
	hpcrun_stream_pre_allocate_metrics(st, 1/*GOLDEN NUMBER*/);
 	st->stream_special_metric_id = hpcrun_stream_new_metric(st);
  hpcrun_stream_set_metric_info_and_period(st, st->stream_special_metric_id, "STREAM SPECIAL (us)", MetricFlags_ValFmt_Int, 1);
	return st;	
}

//duplicate of the function from cct2metrics.c
metric_set_t* hpcrun_stream_reify_metric_set(stream_data_t *st, cct_node_id_t cct_id) {
	metric_set_t* rv = NULL;
	cct2metrics_t* map = st->cct2metrics_map;

	if(map) {
		map = stream_cct_metrics_splay(map, cct_id);
		if(map->node == cct_id) {
  		rv = map->metrics;
		}
	}
	if(rv == NULL) {
		rv = hpcrun_malloc(st->metrics->n_metrics * sizeof(hpcrun_metricVal_t));
		if(!map) {
			map = cct2metrics_new(cct_id, rv); 
		} else {
			cct2metrics_t* new = cct2metrics_new(cct_id, rv);
    	map = stream_cct_metrics_splay(map, cct_id);
			if (map->node == cct_id) { //WHAT? FIXME
      	EMSG("CCT2METRICS map assoc invariant violated");
    	}
    	else {
      	if (map->node < cct_id) {
        	new->left = map; new->right = map->right; map->right = NULL;
      	}
      	else {
        	new->left = map->left; new->right = map; map->left = NULL;
      	}
      	map = new;
    	}
		}
	}
	st->cct2metrics_map = map;
  return rv;
}


cct_node_t *stream_backtrace2cct(stream_data_t *st, ucontext_t *context) {
  backtrace_info_t bt;
	//volatile int DEBUGGING_WAIT=1;
	//while(DEBUGGING_WAIT);
  if (! hpcrun_generate_stream_backtrace(st, context, &bt, 0)) {
    if (ENABLED(NO_PARTIAL_UNW)){
      return NULL;
    }
  }
  frame_t* bt_beg = bt.begin;
  frame_t* bt_last = bt.last;
  //bool tramp_found = bt.has_tramp;
  // If this backtrace is generated from sampling in a thread,
  // take off the top 'monitor_pthread_main' node
  /*if (cct->ctxt && ! partial_unw && ! tramp_found && (bt.fence == FENCE_THREAD)) {
    TMSG(FENCE, "bt last thread correction made");
    TMSG(THREAD_CTXT, "Thread correction, back off outermost backtrace entry");
    bt_last--;
  }*/
	cct_bundle_t* cct= &(st->epoch->csdata);
	//volatile int DEBUGGING_WAIT=1;
	//while(DEBUGGING_WAIT);
  cct_node_t* n = cct_insert_raw_backtrace(cct->tree_root, bt_last, bt_beg);
	//printf("\nAdding a cct node for stream, adding metric data : %x",n);
	hpcrun_stream_get_num_metrics(st);
  //hpcrun_stream_get_metric_proc(st, st->stream_special_metric_id)(st->stream_special_metric_id, hpcrun_stream_reify_metric_set(st, n), (cct_metric_data_t){.i = 424242});
	stream_cct_metric_data_increment(st, st->stream_special_metric_id, n, (cct_metric_data_t) {.i = 42});
	//fprintf(stdout, "How to check if this cct came out correctly %p\n",n);
	return n;
}

int
hpcrun_generate_stream_backtrace(stream_data_t *st, ucontext_t *context, backtrace_info_t *bt, int skipInner) {
				hpcrun_unw_cursor_t cursor;
				hpcrun_unw_init_cursor(&cursor, context);
				st->btbuf_cur   = st->btbuf_beg; // innermost
				st->btbuf_sav   = st->btbuf_end;
				int unw_len = 0;
				step_state ret = STEP_ERROR;;
				while (true) {
								unw_word_t ip;
								hpcrun_unw_get_ip_unnorm_reg(&cursor, &ip);
								hpcrun_ensure_stream_btbuf_avail(st);
								st->btbuf_cur->cursor = cursor;
								hpcrun_unw_get_ip_norm_reg(&st->btbuf_cur->cursor,
																&st->btbuf_cur->ip_norm);
								st->btbuf_cur->ra_loc = NULL;
								void *func_start_pc = NULL, *func_end_pc = NULL;
								load_module_t* lm = NULL;
								fnbounds_enclosing_addr(cursor.pc_unnorm, &func_start_pc, &func_end_pc, &lm);
								st->btbuf_cur->the_function = hpcrun_normalize_ip(func_start_pc, lm);
								frame_t* prev = st->btbuf_cur;
								st->btbuf_cur++;
								unw_len++;
								ret = hpcrun_unw_step(&cursor);
								if (ret == STEP_TROLL) {
												bt->trolled = true;
												bt->n_trolls++;
								}
								if (ret <= 0) {
												if (ret == STEP_ERROR) {
																//hpcrun_stats_num_samples_dropped_inc();
												}
												else { // STEP_STOP
																bt->fence = cursor.fence;
												}
												break;
								}
								prev->ra_loc = hpcrun_unw_get_ra_loc(&cursor);
				}
				frame_t* bt_beg  = st->btbuf_beg;      // innermost, inclusive
				frame_t* bt_last = st->btbuf_cur - 1; // outermost, inclusive

				//skipInner = 3;
				if(skipInner) {
								bt_beg = hpcrun_skip_chords(bt_last, bt_beg, skipInner);
				}
				bt->begin = bt_beg;
				bt->last = bt_last;
				if (! (ret == STEP_STOP)) {
								TMSG(BT, "** Soft Failure **");
								return false;
				}
				TMSG(BT, "succeeds");
				return true;
}


void 
hpcrun_stream_finalize(stream_data_t *st) {
	hpcrun_write_stream_profile_data(st);
}

frame_t*
hpcrun_expand_stream_btbuf(stream_data_t *st)
{
  frame_t* unwind = st->btbuf_cur;

  /* how big is the current buffer? */
  size_t sz = st->btbuf_end - st->btbuf_beg;
  size_t newsz = sz*2;
  /* how big is the current backtrace? */
  size_t btsz = st->btbuf_end - st->btbuf_sav;
  /* how big is the backtrace we're recording? */
  size_t recsz = unwind - st->btbuf_beg;
  /* get new buffer */
  TMSG(EPOCH," epoch_expand_buffer");
  frame_t *newbt = hpcrun_malloc(newsz*sizeof(frame_t));

  if(st->btbuf_sav > st->btbuf_end) {
    EMSG("Invariant btbuf_sav > btbuf_end violated");
    monitor_real_abort();
  }

  /* copy frames from old to new */
  memcpy(newbt, st->btbuf_beg, recsz*sizeof(frame_t));
  memcpy(newbt+newsz-btsz, st->btbuf_end-btsz, btsz*sizeof(frame_t));

  /* setup new pointers */
  st->btbuf_beg = newbt;
  st->btbuf_end = newbt+newsz;
  st->btbuf_sav = newbt+newsz-btsz;

  /* return new unwind pointer */
  return newbt+recsz;
}

void
hpcrun_ensure_stream_btbuf_avail(stream_data_t *st)
{
  if (st->btbuf_cur == st->btbuf_end) {
    st->btbuf_cur = hpcrun_expand_stream_btbuf(st);
    st->btbuf_sav = st->btbuf_end;
  }
}

















