// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2014, Rice University
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


//******************************************************************************
// local includes  
//******************************************************************************

#include <ompt.h>

#include <lib/prof-lean/placeholders.h>
#include <hpcrun/cct_backtrace_finalize.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/trace.h>
#include <hpcrun/unresolved.h>

#include "../hpcrun-initializers.h"

#include "ompt-callstack.h"
#include "ompt-interface.h"
#include "ompt-state-placeholders.h"
#include "ompt-defer.h"
#include "ompt-region.h"
#include "ompt-task-map.h"
#include "ompt-thread.h"

#if defined(HOST_CPU_PPC) 
#include "ppc64-gnu-omp.h"
#elif defined(HOST_CPU_x86) || defined(HOST_CPU_x86_64)
#include "x86-gnu-omp.h"
#else
#error "invalid architecture type"
#endif


//******************************************************************************
// macros
//******************************************************************************

#define OMPT_DEBUG 1

#if OMPT_DEBUG
#define elide_debug_dump(t,i,o,r) if (ompt_callstack_debug) stack_dump(t,i,o,r)
#define elide_frame_dump() if (ompt_callstack_debug) frame_dump()
#else
#define elide_debug_dump(t,i,o,r)
#define elide_frame_dump() if (ompt_callstack_debug) frame_dump()
#endif


//******************************************************************************
// private variables 
//******************************************************************************

static cct_backtrace_finalize_entry_t ompt_finalizer;

static closure_t ompt_callstack_init_closure;

static int ompt_eager_context = 0;
static int ompt_callstack_debug = 0;



//******************************************************************************
// private  operations
//******************************************************************************

static void 
stack_dump(
  char *tag, 
  frame_t *inner, 
  frame_t *outer, 
  uint64_t region_id
) 
{
  EMSG("-----%s start", tag); 
  for (frame_t* x = inner; x <= outer; ++x) {
    void* ip;
    hpcrun_unw_get_ip_unnorm_reg(&(x->cursor), &ip);

    load_module_t* lm = hpcrun_loadmap_findById(x->ip_norm.lm_id);
    const char* lm_name = (lm) ? lm->name : "(null)";

    EMSG("ip = %p (%p), sp = %p, load module = %s", 
	 ip, x->ip_norm.lm_ip, x->cursor.sp, lm_name);
  }
  EMSG("-----%s end", tag); 
  EMSG("<0x%lx>\n", region_id); 
}


static void 
frame_dump() 
{
  EMSG("-----frame start"); 
  for (int i=0;; i++) {
    ompt_frame_t *frame = hpcrun_ompt_get_task_frame(i);
    if (frame == NULL) break;

    void *r = frame->reenter_runtime_frame;
    void *e = frame->exit_runtime_frame;
    EMSG("frame %d: (r=%p, e=%p)", i, r, e);
  }
  EMSG("-----frame end"); 
}


static int
interval_contains(
  void *lower, 
  void *upper, 
  void *addr
)
{
  uint64_t uaddr  = (uint64_t) addr;
  uint64_t ulower = (uint64_t) lower;
  uint64_t uupper = (uint64_t) upper;
  
  return ((ulower <= uaddr) & (uaddr <= uupper));
}


static ompt_state_t 
check_state()
{
  uint64_t wait_id;
  return hpcrun_ompt_get_state(&wait_id);
}


static void 
set_frame(frame_t *f, ompt_placeholder_t *ph)
{
  f->cursor.pc_unnorm = ph->pc;
  f->ip_norm = ph->pc_norm;
}


static void 
collapse_callstack(backtrace_info_t *bt, ompt_placeholder_t *placeholder) 

{
  set_frame(bt->last, placeholder);
  bt->begin = bt->last; 
  bt->bottom_frame_elided = false;
  bt->partial_unwind = false;
  bt->trace_pc = bt->begin->cursor.pc_unnorm;
}


static void
ompt_elide_runtime_frame(
  backtrace_info_t *bt, 
  uint64_t region_id, 
  int isSync
)
{
  // eliding only if the thread is an OpenMP initial or worker thread 
  switch(ompt_thread_type_get()) {
  case ompt_thread_initial:
    break;
  case ompt_thread_worker:
    if (hpcrun_ompt_get_parallel_id(0) != ompt_parallel_id_none) break;
  case ompt_thread_other: 
  case ompt_thread_unknown: 
  default:
    return;
  }

  // collapse callstack if a thread is idle or waiting in a barrier
  switch(check_state()) {
  case ompt_state_wait_barrier:
  case ompt_state_wait_barrier_implicit:
  case ompt_state_wait_barrier_explicit:
    break; // FIXME: skip barrier collapsing until the kinks are worked out.
    collapse_callstack(bt, &ompt_placeholders.ompt_barrier_wait);
    return;
  case ompt_state_idle:
    if (!TD_GET(master)) { 
      collapse_callstack(bt, &ompt_placeholders.ompt_idle);
      return;
    }
  default: break;
  }

  frame_t **bt_outer = &bt->last; 
  frame_t **bt_inner = &bt->begin;

  frame_t *bt_outer_at_entry = *bt_outer;


  int i = 0;
  frame_t *it = NULL;

  ompt_frame_t *frame0 = hpcrun_ompt_get_task_frame(i);

  TD_GET(omp_task_context) = 0;

  elide_debug_dump("ORIGINAL", *bt_inner, *bt_outer, region_id); 
  elide_frame_dump();

  //---------------------------------------------------------------
  // handle all of the corner cases that can occur at the top of 
  // the stack first
  //---------------------------------------------------------------

  if (!frame0) {
    // corner case: the innermost task (if any) has no frame info. 
    // no action necessary. just return.
    goto clip_base_frames;
  }

  while ((frame0->reenter_runtime_frame == 0) && 
         (frame0->exit_runtime_frame == 0)) {

    // corner case: the top frame has been set up, 
    // but not filled in. ignore this frame.
    frame0 = hpcrun_ompt_get_task_frame(++i);

    if (!frame0) {
      // corner case: the innermost task (if any) has no frame info. 
      goto clip_base_frames;
    }
  }

  if (frame0->exit_runtime_frame && 
      (((uint64_t) frame0->exit_runtime_frame) < 
       ((uint64_t) (*bt_inner)->cursor.sp))) {
    // corner case: the top frame has been set up, exit frame has been filled in; 
    // however, exit_runtime_frame points beyond the top of stack. the final call 
    // to user code hasn't been made yet. ignore this frame.
    frame0 = hpcrun_ompt_get_task_frame(++i);
  }

  if (!frame0) {
    // corner case: the innermost task (if any) has no frame info. 
    goto clip_base_frames;
  }

  if (frame0->reenter_runtime_frame) { 
    // the sample was received inside the runtime; 
    // elide frames from top of stack down to runtime entry
    int found = 0;
    for (it = *bt_inner; it <= *bt_outer; it++) {
      if ((uint64_t)(it->cursor.sp) > (uint64_t)frame0->reenter_runtime_frame) {
	if (isSync) {
	  // for synchronous samples, elide runtime frames at top of stack
	  *bt_inner = it;
	}
	found = 1;
	break;
      }
    }

    if (found == 0) {
      // reenter_runtime_frame not found on stack. all frames are runtime frames
      goto clip_base_frames;
    }
    // frames at top of stack elided. continue with the rest
  }

  // general case: elide frames between frame1->enter and frame0->exit
  while (true) {
    frame_t *exit0 = NULL, *reenter1 = NULL;
    ompt_frame_t *frame1;

    frame0 = hpcrun_ompt_get_task_frame(i);

    if (!frame0) break;

    ompt_task_id_t tid = hpcrun_ompt_get_task_id(i);
    cct_node_t *omp_task_context = ompt_task_map_lookup(tid);

    void *low_sp = (*bt_inner)->cursor.sp;
    void *high_sp = (*bt_outer)->cursor.sp;

    // if a frame marker is inside the call stack, set its flag to true
    bool exit0_flag = 
      interval_contains(low_sp, high_sp, frame0->exit_runtime_frame);

    /* start from the top of the stack (innermost frame). 
       find the matching frame in the callstack for each of the markers in the
       stack. look for them in the order in which they should occur.

       optimization note: this always starts at the top of the stack. this can
       lead to quadratic cost. could pick up below where you left off cutting in 
       previous iterations.
    */
    it = *bt_inner; 
    if(exit0_flag) {
      for (; it <= *bt_outer; it++) {
        if((uint64_t)(it->cursor.sp) > (uint64_t)(frame0->exit_runtime_frame)) {
          exit0 = it - 1;
          break;
        }
      }
    }

    if (exit0_flag && omp_task_context) {
      TD_GET(omp_task_context) = omp_task_context;
      *bt_outer = exit0 - 1;
      break;
    }

    frame1 = hpcrun_ompt_get_task_frame(++i);
    if (!frame1) break;

    bool reenter1_flag = 
      interval_contains(low_sp, high_sp, frame1->reenter_runtime_frame); 
    if(reenter1_flag) {
      for (; it <= *bt_outer; it++) {
        if((uint64_t)(it->cursor.sp) > (uint64_t)(frame1->reenter_runtime_frame)) {
          reenter1 = it - 1;
          break;
        }
      }
    }

    if (exit0 && reenter1) {
      // FIXME: IBM and INTEL need to agree
      // laksono 2014.07.08: hack removing one more frame to avoid redundancy with the parent
      // It seems the last frame of the master is the same as the first frame of the workers thread
      // By eliminating the topmost frame we should avoid the appearance of the same frame twice 
      //  in the callpath
      memmove(*bt_inner+(reenter1-exit0+1), *bt_inner, 
	      (exit0 - *bt_inner)*sizeof(frame_t));

      *bt_inner = *bt_inner + (reenter1 - exit0 + 1);
      exit0 = reenter1 = NULL;
    } else if (exit0 && !reenter1) {
      // corner case: reenter1 is in the team master's stack, not mine. eliminate all
      // frames below the exit frame.
      *bt_outer = exit0 - 1;
      break;
    }
  }

  if (*bt_outer != bt_outer_at_entry) {
    bt->bottom_frame_elided = true;
    bt->partial_unwind = false;
  }

  bt->trace_pc = (*bt_inner)->cursor.pc_unnorm;

  elide_debug_dump("ELIDED", *bt_inner, *bt_outer, region_id); 
  return;

 clip_base_frames:
  {
    int master = TD_GET(master);
    if (!master) { 
      set_frame(*bt_outer, &ompt_placeholders.ompt_idle);
      *bt_inner = *bt_outer; 
      bt->bottom_frame_elided = false;
      bt->partial_unwind = false;
      bt->trace_pc = (*bt_inner)->cursor.pc_unnorm;
      return;
    }

    /* runtime frames with nothing else; it is harmless to reveal them all */
    uint64_t idle_frame = (uint64_t) hpcrun_ompt_get_idle_frame();

    if (idle_frame) {
      /* clip below the idle frame */
      for (it = *bt_inner; it <= *bt_outer; it++) {
	if ((uint64_t)(it->cursor.sp) >= idle_frame) {
	  *bt_outer = it - 2;
          bt->bottom_frame_elided = true;
          bt->partial_unwind = true;
	  break;
	}
      }
    } else {
      /* no idle frame. show the whole stack. */
    }
    
    elide_debug_dump("ELIDED INNERMOST FRAMES", *bt_inner, *bt_outer, region_id); 
    return;
  }
}


static cct_node_t *
memoized_context_get(thread_data_t* td, uint64_t region_id)
{
  return (td->outer_region_id == region_id && td->outer_region_context) ? 
    td->outer_region_context : 
    NULL;
}

static void
memoized_context_set(thread_data_t* td, uint64_t region_id, cct_node_t *result)
{
    td->outer_region_id = region_id;
    td->outer_region_context = result;
}


cct_node_t *
region_root(cct_node_t *_node)
{
  cct_node_t *root;
  cct_node_t *node = _node;
  while (node) {
    cct_addr_t *addr = hpcrun_cct_addr(node);
    if (IS_UNRESOLVED_ROOT(addr)) {
      root = hpcrun_get_thread_epoch()->csdata.unresolved_root; 
      break;
    } else if (IS_PARTIAL_ROOT(addr)) {
      root = hpcrun_get_thread_epoch()->csdata.partial_unw_root; 
      break;
    }
    node = hpcrun_cct_parent(node);
  }
  if (node == NULL) root = hpcrun_get_thread_epoch()->csdata.tree_root; 
  return root;
}

static cct_node_t *
lookup_region_id(uint64_t region_id)
{
  thread_data_t* td = hpcrun_get_thread_data();
  cct_node_t *result = NULL;

  if (hpcrun_trace_isactive()) {
    result = memoized_context_get(td, region_id);
    if (result) return result;
  
    cct_node_t *t0_path = hpcrun_region_lookup(region_id);
    if (t0_path) {
      cct_node_t *rroot = region_root(t0_path);
      result = hpcrun_cct_insert_path_return_leaf(rroot, t0_path);
      memoized_context_set(td, region_id, result);
    }
  }

  return result;
}


cct_node_t *
ompt_region_context(uint64_t region_id, 
		    ompt_context_type_t ctype, 
		    int levels_to_skip,
                    int adjust_callsite)
{
  cct_node_t *node;
  ucontext_t uc;
  getcontext(&uc);

#if 0
  node = hpcrun_sample_callpath(&uc, 0, 0, ++levels_to_skip, 1).sample_node;
#else
  // levels to skip will be broken if inlining occurs.
  node = hpcrun_sample_callpath(&uc, 0, 0, 0, 1).sample_node;
#endif
  TMSG(DEFER_CTXT, "unwind the callstack for region 0x%lx", region_id);

  if (node && adjust_callsite) {
    // extract the load module and offset of the leaf CCT node at the 
    // end of a call path representing a parallel region
    cct_addr_t *n = hpcrun_cct_addr(node);
    cct_node_t *n_parent = hpcrun_cct_parent(node); 
    uint16_t lm_id = n->ip_norm.lm_id; 
    uintptr_t lm_ip = n->ip_norm.lm_ip;
    uintptr_t master_outlined_fn_return_addr;

    // adjust the address to point to return address of the call to 
    // the outlined function in the master
    if (ctype == ompt_context_begin) {
      void *ip = hpcrun_denormalize_ip(&(n->ip_norm));
      uint64_t offset = offset_to_pc_after_next_call(ip);
      master_outlined_fn_return_addr = lm_ip + offset;
    } else { 
      uint64_t offset = length_of_call_instruction();
      master_outlined_fn_return_addr = lm_ip - offset;
    }
    // ensure that there is a leaf CCT node with the proper return address
    // to use as the context. when using the GNU API for OpenMP, it will 
    // be a sibling to one returned by sample_callpath.
    cct_node_t *sibling = hpcrun_cct_insert_addr
      (n_parent, &(ADDR2(lm_id, master_outlined_fn_return_addr)));
    node = sibling;
  }

  return node;
}

cct_node_t *
ompt_parallel_begin_context(ompt_parallel_id_t region_id, int levels_to_skip, 
                            int adjust_callsite)
{
  if (ompt_eager_context) { 
    return ompt_region_context(region_id, ompt_context_begin, 
                               ++levels_to_skip, adjust_callsite);
  } else {
    return NULL;
  }
}


static void
ompt_backtrace_finalize(
  backtrace_info_t *bt, 
  int isSync
) 
{
  // ompt: elide runtime frames
  // if that is the case, then it will later become available in a deferred fashion.
  int master = TD_GET(master);
  if (!master) {
    if (need_defer_cntxt()) {
      resolve_cntxt();
    }
  }
  uint64_t region_id = TD_GET(region_id);

  ompt_elide_runtime_frame(bt, region_id, isSync);
}



//******************************************************************************
// interface operations
//******************************************************************************

cct_node_t *
ompt_cct_cursor_finalize(cct_bundle_t *cct, backtrace_info_t *bt, 
                           cct_node_t *cct_cursor)
{
  cct_node_t *omp_task_context = TD_GET(omp_task_context);

  // FIXME: should memoize the resulting task context in a thread-local variable
  //        I think we can just return omp_task_context here. it is already
  //        relative to one root or another.
  if (omp_task_context) {
    cct_node_t *root;
#if 1
    root = region_root(omp_task_context);
#else
    if((is_partial_resolve((cct_node_t *)omp_task_context) > 0)) {
      root = hpcrun_get_thread_epoch()->csdata.unresolved_root; 
    } else {
      root = hpcrun_get_thread_epoch()->csdata.tree_root; 
    }
#endif
    return hpcrun_cct_insert_path_return_leaf(root, omp_task_context);
  }

  // if I am not the master thread, full context may not be immediately available.
  // if that is the case, then it will later become available in a deferred fashion.
  if (!TD_GET(master)) { // sub-master thread in nested regions
    uint64_t region_id = TD_GET(region_id);
    // FIXME: check whether bottom frame elided will be right for IBM runtime
    //        without help of get_idle_frame
    if (region_id > 0 && bt->bottom_frame_elided) {

      cct_node_t *prefix = lookup_region_id(region_id);
      if (prefix) {
	// full context is available now. use it.
	cct_cursor = prefix;
      } else {
	// full context is not available. if the there is a node for region_id in 
	// the unresolved tree, use it as the cursor to anchor the sample for now. 
	// it will be resolved later. otherwise, use the default cursor.
	prefix = 
	  hpcrun_cct_find_addr((hpcrun_get_thread_epoch()->csdata).unresolved_root, 
			       &(ADDR2(UNRESOLVED, region_id)));
	if (prefix) cct_cursor = prefix;
      }
    }
  }

  return cct_cursor;
}

void
ompt_callstack_init_deferred(void)
{
  if (hpcrun_trace_isactive()) ompt_eager_context = 1;
}

void
ompt_callstack_init(void)
{
  ompt_finalizer.next = 0;
  ompt_finalizer.fn = ompt_backtrace_finalize;
  cct_backtrace_finalize_register(&ompt_finalizer);
  cct_cursor_finalize_register(ompt_cct_cursor_finalize);

  // initialize closure for initializer
  ompt_callstack_init_closure.fn = 
    (closure_fn_t) ompt_callstack_init_deferred; 
  ompt_callstack_init_closure.arg = 0;

  // register closure
  hpcrun_initializers_defer(&ompt_callstack_init_closure);
}

