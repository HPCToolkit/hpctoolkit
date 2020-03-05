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
// Copyright ((c)) 2002-2020, Rice University
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

#include <hpcrun/cct_backtrace_finalize.h>
#include <hpcrun/hpcrun-initializers.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/thread_finalize.h>
#include <hpcrun/trace.h>
#include <hpcrun/unresolved.h>

#include "ompt-callstack.h"
#include "ompt-defer.h"
#include "ompt-interface.h"
#include "ompt-placeholders.h"
#include "ompt-thread.h"

#if defined(HOST_CPU_PPC) 
#include "ompt-gcc4-ppc64.h"
#define ADJUST_PC
#elif defined(HOST_CPU_x86) || defined(HOST_CPU_x86_64)
#include "ompt-gcc4-x86.h"
#define ADJUST_PC
#elif defined(HOST_CPU_ARM64)
#else
#error "invalid architecture type"
#endif



//******************************************************************************
// macros
//******************************************************************************

#define DEREFERENCE_IF_NON_NULL(ptr) (ptr ? *(void **) ptr : 0)

#define OMPT_DEBUG 0
#define ALLOW_DEFERRED_CONTEXT 0


#if OMPT_DEBUG
#define elide_debug_dump(t, i, o, r) \
  if (ompt_callstack_debug) stack_dump(t, i, o, r)
#define elide_frame_dump() if (ompt_callstack_debug) frame_dump()
#else
#define elide_debug_dump(t, i, o, r)
#define elide_frame_dump() 
#endif


#define FP(frame, which) (frame->which ## _frame.ptr)
#define FF(frame, which) (frame->which ## _frame_flags)

#define ff_is_appl(flags) (flags & ompt_frame_application)
#define ff_is_rt(flags)   (!ff_is_appl(flags))
#define ff_is_fp(flags)   (flags & ompt_frame_framepointer)



//******************************************************************************
// private variables 
//******************************************************************************

static cct_backtrace_finalize_entry_t ompt_finalizer;

#if ALLOW_DEFERRED_CONTEXT
static thread_finalize_entry_t ompt_thread_finalizer;
#endif

static closure_t ompt_callstack_init_closure;

static int ompt_eager_context = 0;

#if OMPT_DEBUG
static int ompt_callstack_debug = 0;
#endif



//******************************************************************************
// private  operations
//******************************************************************************

static void *
fp_exit
(
 ompt_frame_t *frame
)
{
  void *ptr = FP(frame, exit);
#if defined(HOST_CPU_PPC) 
  int flags = FF(frame, exit);
  // on power: ensure the enter frame pointer is CFA for runtime frame
  if (ff_is_fp(flags)) {
    ptr = DEREFERENCE_IF_NON_NULL(ptr);
  }
#endif
  return ptr;
}


static void *
fp_enter
(
 ompt_frame_t *frame
)
{
  void *ptr = FP(frame, enter);
#if defined(HOST_CPU_PPC) 
  int flags = FF(frame, enter);
  // on power: ensure the enter frame pointer is CFA for runtime frame
  if (ff_is_fp(flags) && ff_is_rt(flags)) {
    ptr = DEREFERENCE_IF_NON_NULL(ptr);
  }
#endif
  return ptr;
}


static void 
__attribute__ ((unused))
stack_dump
(
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
__attribute__ ((unused))
frame_dump
(
 void
) 
{
  EMSG("-----frame start");
  for (int i=0;; i++) {
    ompt_frame_t *frame = hpcrun_ompt_get_task_frame(i);
    if (frame == NULL) break;

    void *ep = fp_enter(frame);
    int ef = FF(frame, enter);

    void *xp = fp_exit(frame);
    int xf = FF(frame, exit);

    EMSG("frame %d: enter=(%p,%x), exit=(%p,%x)", i, ep, ef, xp, xf);
  }
  EMSG("-----frame end"); 
}


static int
interval_contains
(
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
check_state
(
 void
)
{
  uint64_t wait_id;
  return hpcrun_ompt_get_state(&wait_id);
}


static void 
set_frame
(
 frame_t *f, 
 ompt_placeholder_t *ph
)
{
  f->cursor.pc_unnorm = ph->pc;
  f->ip_norm = ph->pc_norm;
  f->the_function = ph->pc_norm;
}


static void
collapse_callstack
(
 backtrace_info_t *bt, 
 ompt_placeholder_t *placeholder
)
{
  set_frame(bt->last, placeholder);
  bt->begin = bt->last;
  bt->bottom_frame_elided = false;
  bt->partial_unwind = false;
  bt->collapsed = true;
//  bt->fence = FENCE_MAIN;
}


static void
ompt_elide_runtime_frame(
  backtrace_info_t *bt, 
  uint64_t region_id, 
  int isSync
)
{
  frame_t **bt_outer = &bt->last;
  frame_t **bt_inner = &bt->begin;

  frame_t *bt_outer_at_entry = *bt_outer;

  ompt_thread_t thread_type = ompt_thread_type_get();
  switch(thread_type) {
  case ompt_thread_initial:
    break;
  case ompt_thread_worker:
    break;
  case ompt_thread_other:
  case ompt_thread_unknown:
  default:
    goto return_label;
  }

  // collapse callstack if a thread is idle or waiting in a barrier
  switch(check_state()) {
    case ompt_state_wait_barrier:
    case ompt_state_wait_barrier_implicit:
    case ompt_state_wait_barrier_explicit:
      // collapse barriers on non-master ranks 
      if (hpcrun_ompt_get_thread_num(0) != 0) {
	collapse_callstack(bt, &ompt_placeholders.ompt_barrier_wait_state);
	goto return_label;
      }
      break; 
    case ompt_state_idle:
      // collapse idle state
      TD_GET(omp_task_context) = 0;
      collapse_callstack(bt, &ompt_placeholders.ompt_idle_state);
      goto return_label;
    default:
      break;
  }

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

  while ((fp_enter(frame0) == 0) && 
         (fp_exit(frame0) == 0)) {
    // corner case: the top frame has been set up, 
    // but not filled in. ignore this frame.
    frame0 = hpcrun_ompt_get_task_frame(++i);

    if (!frame0) {
      if (thread_type == ompt_thread_initial) return;

      // corner case: the innermost task (if any) has no frame info. 
      goto clip_base_frames;
    }
  }

  if (fp_exit(frame0) &&
      (((uint64_t) fp_exit(frame0)) <
        ((uint64_t) (*bt_inner)->cursor.sp))) {
    // corner case: the top frame has been set up, exit frame has been filled in; 
    // however, exit_frame.ptr points beyond the top of stack. the final call 
    // to user code hasn't been made yet. ignore this frame.
    frame0 = hpcrun_ompt_get_task_frame(++i);
  }

  if (!frame0) {
    // corner case: the innermost task (if any) has no frame info. 
    goto clip_base_frames;
  }

  if (fp_enter(frame0)) { 
    // the sample was received inside the runtime; 
    // elide frames from top of stack down to runtime entry
    int found = 0;
    for (it = *bt_inner; it <= *bt_outer; it++) {
      if ((uint64_t)(it->cursor.sp) >= (uint64_t)fp_enter(frame0)) {
        if (isSync) {
          // for synchronous samples, elide runtime frames at top of stack
          *bt_inner = it;
        }
        found = 1;
        break;
      }
    }

    if (found == 0) {
      // enter_frame not found on stack. all frames are runtime frames
      goto clip_base_frames;
    }
    // frames at top of stack elided. continue with the rest
  }

  // FIXME vi3: trouble with master thread when defering
  // general case: elide frames between frame1->enter and frame0->exit
  while (true) {
    frame_t *exit0 = NULL, *reenter1 = NULL;
    ompt_frame_t *frame1;

    frame0 = hpcrun_ompt_get_task_frame(i);

    if (!frame0) break;

    ompt_data_t *task_data = hpcrun_ompt_get_task_data(i);
    cct_node_t *omp_task_context = NULL;
    if (task_data)
      omp_task_context = task_data->ptr;
    
    void *low_sp = (*bt_inner)->cursor.sp;
    void *high_sp = (*bt_outer)->cursor.sp;

    // if a frame marker is inside the call stack, set its flag to true
    bool exit0_flag = 
      interval_contains(low_sp, high_sp, fp_exit(frame0));

    /* start from the top of the stack (innermost frame). 
       find the matching frame in the callstack for each of the markers in the
       stack. look for them in the order in which they should occur.

       optimization note: this always starts at the top of the stack. this can
       lead to quadratic cost. could pick up below where you left off cutting in 
       previous iterations.
    */
    it = *bt_inner; 
    if (exit0_flag) {
      for (; it <= *bt_outer; it++) {
        if ((uint64_t)(it->cursor.sp) >= (uint64_t)(fp_exit(frame0))) {
          int offset = ff_is_appl(FF(frame0, exit)) ? 0 : 1;
          exit0 = it - offset;
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

    //-------------------------------------------------------------------------
    //  frame1 points into the stack above the task frame (in the
    //  runtime from the outer task's perspective). frame0 points into
    //  the the stack inside the first application frame (in the
    //  application from the inner task's perspective) the two points
    //  are equal. there is nothing to elide at this step.
    //-------------------------------------------------------------------------
    if ((fp_enter(frame1) == fp_exit(frame0)) &&
	(ff_is_appl(FF(frame0, exit)) &&
	 ff_is_rt(FF(frame1, enter))))
      continue;


    bool reenter1_flag = 
      interval_contains(low_sp, high_sp, fp_enter(frame1));

#if 0
    ompt_frame_t *help_frame = region_stack[top_index-i+1].parent_frame;
    if (!ompt_eager_context && !reenter1_flag && help_frame) {
      frame1 = help_frame;
      reenter1_flag = interval_contains(low_sp, high_sp, fp_enter(frame1));
      // printf("THIS ONLY HAPPENS IN MASTER: %d\n", TD_GET(master));
    }
#endif

    if (reenter1_flag) {
      for (; it <= *bt_outer; it++) {
        if ((uint64_t)(it->cursor.sp) >= (uint64_t)(fp_enter(frame1))) {
          reenter1 = it - 1;
          break;
        }
      }
    }



    // FIXME vi3: This makes trouble with master thread when defering
    if (exit0 && reenter1) {



      // FIXME: IBM and INTEL need to agree
      // laksono 2014.07.08: hack removing one more frame to avoid redundancy with the parent
      // It seems the last frame of the master is the same as the first frame of the workers thread
      // By eliminating the topmost frame we should avoid the appearance of the same frame twice 
      //  in the callpath

      // FIXME vi3: find better way to solve this  "This makes trouble with master thread when defering"
//      if (TD_GET(master)){
//        return;
//      }
//      if (omp_get_thread_num() == 0)
//        return;

      //------------------------------------
      // The prefvous version DON'T DELETE
      memmove(*bt_inner+(reenter1-exit0+1), *bt_inner,
	      (exit0 - *bt_inner)*sizeof(frame_t));

      *bt_inner = *bt_inner + (reenter1 - exit0 + 1);

      exit0 = reenter1 = NULL;
      // --------------------------------

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
    if (*bt_outer < *bt_inner) {
      //------------------------------------------------------------------------
      // corner case: 
      //   the thread state is not ompt_state_idle, but we are eliding the 
      //   whole call stack anyway. 
      // how this arises:
      //   when a sample is delivered between when a worker thread's task state 
      //   is set to ompt_state_work_parallel but the user outlined function 
      //   has not yet been invoked. 
      // considerations:
      //   bt_outer may be out of bounds 
      // handling:
      //   (1) reset both cursors to something acceptable
      //   (2) collapse context to an <openmp idle>
      //------------------------------------------------------------------------
      *bt_outer = *bt_inner;
      TD_GET(omp_task_context) = 0;
      collapse_callstack(bt, &ompt_placeholders.ompt_idle_state);
    }
  }

  elide_debug_dump("ELIDED", *bt_inner, *bt_outer, region_id);
  goto return_label;

 clip_base_frames:
  {
    int master = TD_GET(master);
    if (!master) {
      set_frame(*bt_outer, &ompt_placeholders.ompt_idle_state);
      *bt_inner = *bt_outer;
      bt->bottom_frame_elided = false;
      bt->partial_unwind = false;
      goto return_label;
    }

#if 0
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
    goto return_label;
#endif
  }


 return_label:
    return;
}


cct_node_t *
ompt_region_root
(
 cct_node_t *_node
)
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


//-------------------------------------------------------------------------------
// Unlike other compilers, gcc4 generates code to invoke the master
// task from the program itself rather than the runtime, as shown 
// in the sketch below
//
// user_function_f () 
// {
//  ...
//         omp_parallel_start(task, ...)
//  label_1:
//         task(...)
//  label_2: 
//         omp_parallel_end(task, ...)
//  label_3: 
//  ...
// }
//
// As a result, unwinding from within a callback from either parallel_start or 
// parallel_end will return an address marked by label_1 or label_2. unwinding
// on the master thread from within task will have label_2 as a return address 
// on its call stack.
//
// Cope with the lack of an LCA by adjusting unwinds from within callbacks 
// when entering or leaving a task by moving the leaf of the unwind representing 
// the region within user_function_f to label_2. 
//-------------------------------------------------------------------------------
static cct_node_t *
ompt_adjust_calling_context
(
 cct_node_t *node,
 ompt_scope_endpoint_t se_type
)
{
#ifdef ADJUST_PC
  // extract the load module and offset of the leaf CCT node at the 
  // end of a call path representing a parallel region
  cct_addr_t *n = hpcrun_cct_addr(node);
  cct_node_t *n_parent = hpcrun_cct_parent(node); 
  uint16_t lm_id = n->ip_norm.lm_id; 
  uintptr_t lm_ip = n->ip_norm.lm_ip;
  uintptr_t master_outlined_fn_return_addr;

  // adjust the address to point to return address of the call to 
  // the outlined task in the master
  if (se_type == ompt_scope_begin) {
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
  return sibling;
#else
  return node;
#endif
}


cct_node_t *
ompt_region_context_eager
(
  uint64_t region_id, 
  ompt_scope_endpoint_t se_type, 
  int adjust_callsite
)
{
  cct_node_t *node;
  ucontext_t uc;
  getcontext(&uc);

  // levels to skip will be broken if inlining occurs.
  hpcrun_metricVal_t zero = {.i = 0};
  node = hpcrun_sample_callpath(&uc, 0, zero, 0, 1, NULL).sample_node;
  TMSG(DEFER_CTXT, "unwind the callstack for region 0x%lx", region_id);

  TMSG(DEFER_CTXT, "unwind the callstack for region 0x%lx", region_id);
  if (node && adjust_callsite) {
    node = ompt_adjust_calling_context(node, se_type);
  }
  return node;
}


void
ompt_region_context_lazy
(
  uint64_t region_id,
  ompt_scope_endpoint_t se_type, 
  int adjust_callsite
)
{
  ucontext_t uc;
  getcontext(&uc);

  hpcrun_metricVal_t blame_metricVal;
  blame_metricVal.i = 0;

  // side-effect: set region context
  (void) hpcrun_sample_callpath(&uc, 0, blame_metricVal, 0, 33, NULL);

  TMSG(DEFER_CTXT, "unwind the callstack for region 0x%lx", region_id);
}


cct_node_t *
ompt_parallel_begin_context
(
 ompt_id_t region_id, 
 int adjust_callsite
)
{
  cct_node_t *context = NULL;
  if (ompt_eager_context) {
    context = ompt_region_context_eager(region_id, ompt_scope_begin,
                                  adjust_callsite);
  }
  return context;
}


static void
ompt_backtrace_finalize
(
 backtrace_info_t *bt, 
 int isSync
) 
{
  // ompt: elide runtime frames
  // if that is the case, then it will later become available in a deferred fashion.
  int master = TD_GET(master);
  if (!master) {
    if (need_defer_cntxt()) {
//      resolve_cntxt();
    }
    if (!ompt_eager_context)
      resolve_cntxt();
  }
  uint64_t region_id = TD_GET(region_id);

  ompt_elide_runtime_frame(bt, region_id, isSync);

  if(!isSync && !ompt_eager_context_p() && !bt->collapsed){
    register_to_all_regions();
  }
}



//******************************************************************************
// interface operations
//******************************************************************************

cct_node_t *
ompt_cct_cursor_finalize
(
 cct_bundle_t *cct, 
 backtrace_info_t *bt, 
 cct_node_t *cct_cursor
)
{

  // when providing a path for the region in which we had a task
  if (!ompt_eager_context && ending_region) {
    return TD_GET(master) ? cct_cursor : cct_not_master_region;
  }

  cct_node_t *omp_task_context = TD_GET(omp_task_context);

  // FIXME: should memoize the resulting task context in a thread-local variable
  //        I think we can just return omp_task_context here. it is already
  //        relative to one root or another.
  if (omp_task_context && omp_task_context != task_data_invalid) {
    cct_node_t *root;
#if 1
    root = ompt_region_root(omp_task_context);
#else
    if ((is_partial_resolve((cct_node_tt *)omp_task_context) > 0)) {
      root = hpcrun_get_thread_epoch()->csdata.unresolved_root;
    } else {
      root = hpcrun_get_thread_epoch()->csdata.tree_root;
    }
#endif
    // FIXME: vi3 why is this called here??? Makes troubles for worker thread when !ompt_eager_context
    if (ompt_eager_context || TD_GET(master))
      return hpcrun_cct_insert_path_return_leaf(root, omp_task_context);
  } else if (omp_task_context && omp_task_context == task_data_invalid) {
    region_stack_el_t *stack_el = &region_stack[top_index];
    ompt_notification_t *notification = stack_el->notification;
    // if no unresolved cct placeholder for the region, create it
    if (!notification->unresolved_cct) {
      cct_node_t *new_cct =
              hpcrun_cct_insert_addr(cct->thread_root, &ADDR2(UNRESOLVED, notification->region_data->region_id));
      notification->unresolved_cct = new_cct;
    }
    // return a placeholder for the sample taken inside tasks
    return notification->unresolved_cct;
  }

  // FIXME: vi3 consider this when tracing, for now everything works fine
  // if I am not the master thread, full context may not be immediately available.
  // if that is the case, then it will later become available in a deferred fashion.
  if (!TD_GET(master)) { // sub-master thread in nested regions

//    uint64_t region_id = TD_GET(region_id);
//    ompt_data_t* current_parallel_data = TD_GET(current_parallel_data);
//    ompt_region_data_t* region_data = (ompt_region_data_t*)current_parallel_data->ptr;
    // FIXME: check whether bottom frame elided will be right for IBM runtime
    //        without help of get_idle_frame

    if (not_master_region && bt->bottom_frame_elided){
      // it should be enough just to set cursor to unresolved node
      // which corresponds to not_master_region

      // everything is ok with cursos
      cct_cursor = cct_not_master_region;

// johnmc merge
#if 0
    if (region_id > 0 && bt->bottom_frame_elided) {

      cct_node_t *prefix = lookup_region_id(region_id);
      if (prefix) {
	      // full context is available now. use it.
	      cct_cursor = prefix;
      } else {
	      // full context is not available. if the there is a node for region_id in 
	      // the unresolved tree, use it as the cursor to anchor the sample for now. 
	      // it will be resolved later. otherwise, use the default cursor.
	      prefix = hpcrun_cct_find_addr((hpcrun_get_thread_epoch()->csdata).unresolved_root,
          &(ADDR2(UNRESOLVED, region_id)));
	      if (prefix) cct_cursor = prefix;
      }
#endif
    }
  }

  return cct_cursor;
}


void
ompt_callstack_init_deferred
(
 void
)
{
#if ALLOW_DEFERRED_CONTEXT
  if (hpcrun_trace_isactive()) ompt_eager_context = 1;
  else {
    // set up a finalizer to propagate information about
    // openmp region contexts that have not been fully 
    // resolved across all threads.
    ompt_thread_finalizer.next = 0;
    ompt_thread_finalizer.fn = ompt_resolve_region_contexts;
    thread_finalize_register(&ompt_thread_finalizer);
  }
#else
  ompt_eager_context = 1;
#endif
}


void
ompt_callstack_init
(
 void
)
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


int
ompt_eager_context_p
(
 void
)
{
  return ompt_eager_context;
}

