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
//
// This software was produced with support in part from the Defense Advanced
// Research Projects Agency (DARPA) through AFRL Contract FA8650-09-C-1915. 
// Nothing in this work should be construed as reflecting the official policy or
// position of the Defense Department, the United States government, or
// Rice University.
//
//***************************************************************************

//************************************************
// system includes
//************************************************

#define UNW_LOCAL_ONLY

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#include <ucontext.h>

#include <include/hpctoolkit-config.h>

//************************************************
// external includes
//************************************************

#include <monitor.h>

//************************************************
// local includes
//************************************************

#include <fnbounds/fnbounds_interface.h>
#include <messages/messages.h>
#include <hpcrun/hpcrun_stats.h>
#include <unwind/common/unw-datatypes.h>
#include <unwind/common/unwind.h>
#include <unwind/common/uw_recipe_map.h>
#include <unwind/common/binarytree_uwi.h>
#include <utilities/arch/context-pc.h>



//************************************************
// macros
//************************************************

#define DEBUG_LIBUNWIND_INTERFACE 0



//************************************************
// local data
//************************************************

#if DEBUG_LIBUNWIND_INTERFACE
static int libunwind_debug = 0;
#endif



//************************************************
// debugging public interface
//************************************************

#if DEBUG_LIBUNWIND_INTERFACE
int 
libunw_debug_on()
{
  libunwind_debug = 1;
  return libunwind_debug;
}


int 
libunwind_debug_off()
{
  libunwind_debug = 0;
  return libunwind_debug;
}
#endif



//************************************************
// debugging private functions
//************************************************

#if DEBUG_LIBUNWIND_INTERFACE
static const char *
fence_string(hpcrun_unw_cursor_t* cursor)
{
  switch(cursor->fence) {
  case FENCE_MAIN: return "FENCE_MAIN";
  case FENCE_NONE: return "FENCE_NONE";
  case FENCE_THREAD: return "FENCE_THREAD";
  case FENCE_BAD: return "FENCE_BAD";
  case FENCE_TRAMP: return "FENCE_TRAMP";
  }
  return NULL;
}

static const char *
step_state_string(step_state state)
{
  switch(state) {
  case STEP_OK: return "STEP_OK";
  case STEP_ERROR: return "STEP_ERROR";
  case STEP_STOP: return "STEP_STOP";
  case STEP_STOP_WEAK: return "STEP_STOP_WEAK";
  case STEP_TROLL: return "STEP_TROLL";
  }
  return NULL;
}

static const char *
libunw_state_string(enum libunw_state state)
{
  switch(state) {
  case LIBUNW_READY: return "LIBUNW_READY";
  case LIBUNW_UNAVAIL: return "LIBUNW_UNAVAIL";
  }
  return NULL;
}
#endif


//************************************************
// private functions
//************************************************

static void *
libunw_cursor_get_pc(hpcrun_unw_cursor_t* cursor)
{
  unw_word_t tmp;

  unw_cursor_t *unw_cursor = &(cursor->uc);
  unw_get_reg(unw_cursor, UNW_REG_IP, &tmp);

  return (void *) tmp;
}


static void *
libunw_cursor_get_sp(hpcrun_unw_cursor_t* cursor)
{
  unw_word_t tmp;

  unw_cursor_t *unw_cursor = &(cursor->uc);
  unw_get_reg(unw_cursor, UNW_REG_SP, &tmp);

  return (void *) tmp;
}


static void *
libunw_cursor_get_bp(hpcrun_unw_cursor_t* cursor)
{
  unw_word_t tmp;

#if HOST_CPU_x86_64
  unw_cursor_t *unw_cursor = &(cursor->uc);
  unw_get_reg(unw_cursor, UNW_TDEP_BP, &tmp);
#else
  tmp = 0;
#endif

  return (void *) tmp;
}


static void **
libunw_cursor_get_ra_loc(hpcrun_unw_cursor_t* cursor)
{
  unw_save_loc_t ip_loc;

  unw_cursor_t *unw_cursor = &(cursor->uc);
  unw_get_save_loc(unw_cursor, UNW_REG_IP, &ip_loc);

  unw_word_t tmp = (ip_loc.type == UNW_SLT_MEMORY ? ip_loc.u.addr : 0);

  return (void **) tmp;
}



static void
compute_normalized_ips(hpcrun_unw_cursor_t* cursor)
{
  void *func_start_pc =  (void*) cursor->unwr_info.interval.start;
  load_module_t* lm = cursor->unwr_info.lm;

  cursor->pc_norm = hpcrun_normalize_ip(cursor->pc_unnorm, lm);
  cursor->the_function = hpcrun_normalize_ip(func_start_pc, lm);

#if DEBUG_LIBUNWIND_INTERFACE
  if (libunwind_debug) {
	printf("ip = %p (%d, %lx) the_function=%p (%d,%lx) ", 
	cursor->pc_unnorm, cursor->pc_norm.lm_id, cursor->pc_norm.lm_ip, 
        func_start_pc, cursor->the_function.lm_id, cursor->the_function.lm_ip);
  }
#endif
}

bool
libunw_finalize_cursor(hpcrun_unw_cursor_t* cursor, int decrement_pc)
{
  char *pc = libunw_cursor_get_pc(cursor);
  cursor->pc_unnorm = pc;

  cursor->sp = libunw_cursor_get_sp(cursor);
  cursor->bp = libunw_cursor_get_bp(cursor);
  cursor->ra_loc = libunw_cursor_get_ra_loc(cursor);

  if (decrement_pc) pc--;
  bool found = uw_recipe_map_lookup(pc, DWARF_UNWINDER, &cursor->unwr_info);

  compute_normalized_ips(cursor);
  TMSG(UNW, "unw_step: advance pc: %p\n", pc);
  cursor->libunw_status = found ? LIBUNW_READY : LIBUNW_UNAVAIL;
  
#if DEBUG_LIBUNWIND_INTERFACE
  if (libunwind_debug) {
    printf("libunw_status = %s ", libunw_state_string(cursor->libunw_status));
  }
#endif

  return found;
}

step_state
libunw_take_step(hpcrun_unw_cursor_t* cursor)
{
  void *pc = libunw_cursor_get_pc(cursor); // pc after step

  // full unwind: stop at libmonitor fence.  this is where we hope the
  // unwind stops.
  cursor->fence = (monitor_unwind_process_bottom_frame(pc) ? FENCE_MAIN :
		   monitor_unwind_thread_bottom_frame(pc)? FENCE_THREAD : FENCE_NONE);

#if DEBUG_LIBUNWIND_INTERFACE
  if (libunwind_debug) {
    printf("fence = %s ", fence_string(cursor));
  }
#endif

  if (cursor->fence != FENCE_NONE) {
    TMSG(UNW, "unw_step: stop at monitor fence: %p\n", pc);
    return STEP_STOP;
  }

  bitree_uwi_t* uw = cursor->unwr_info.btuwi;
  if (!uw) {
    // If we don't have unwind info, let libunwind do its thing.
    int ret = unw_step(&(cursor->uc));
    if(ret > 0) return STEP_OK;
    // Libunwind failed (or the frame-chain ended). Log an error and error.
    switch(-ret) {
    case 0:
      TMSG(UNW, "libunw_take_step: error: frame-chain ended at %p\n", pc);
      break;
    case UNW_EUNSPEC:
      TMSG(UNW, "libunw_take_step: error: unspecified error at %p\n", pc);
      break;
    case UNW_ENOINFO:
      TMSG(UNW, "libunw_take_step: error: no unwind info at %p\n", pc);
      break;
    case UNW_EBADVERSION:
      TMSG(UNW, "libunw_take_step: error: unreadable unwind info at %p\n", pc);
      break;
    case UNW_EINVALIDIP:
      TMSG(UNW, "libunw_take_step: error: invalid pc at %p\n", pc);
      break;
    case UNW_EBADFRAME:
      TMSG(UNW, "libunw_take_step: error: bad frame at %p\n", pc);
      break;
    case UNW_ESTOPUNWIND:
      TMSG(UNW, "libunw_take_step: error: libunwind stopped unwind at %p\n", pc);
      break;
    default:
      TMSG(UNW, "libunw_take_step: error: unknown libunwind error at %p\n", pc);
      break;
    }
    return STEP_ERROR;
  }

  uwi_t *uwi = bitree_uwi_rootval(uw);
  unw_apply_reg_state(&(cursor->uc), uwi->recipe);

  return STEP_OK;
}

// Takes a sigaction() context pointer and makes a libunwind
// unw_cursor_t.  The other fields in hpcrun_unw_cursor_t are unused,
// except for pc_norm and pc_unnorm.
//
void
libunw_unw_init_cursor(hpcrun_unw_cursor_t* cursor, void* context)
{
  unw_cursor_t *unw_cursor = &(cursor->uc);
  unw_context_t *ctx = (unw_context_t *) context;

  if (ctx != NULL && unw_init_local2(unw_cursor, ctx, UNW_INIT_SIGNAL_FRAME) == 0) {
    libunw_finalize_cursor(cursor, 0);
  }
}

//***************************************************************************
// unwind_interval interface
//***************************************************************************

struct builder
{
  unwinder_t uw;
  bitree_uwi_t *latest;
  int count;
};

static int
dwarf_reg_states_callback(void *token,
			  void *rs,
			  size_t size,
			  unw_word_t start_ip, unw_word_t end_ip)
{
  struct builder *b = token;
  bitree_uwi_t *u = bitree_uwi_malloc(b->uw, size);
  if (!u)
    return (1);
  bitree_uwi_set_rightsubtree(b->latest, u);
  uwi_t *uwi =  bitree_uwi_rootval(u);
  uwi->interval.start = (uintptr_t)start_ip;
  uwi->interval.end = (uintptr_t)end_ip;
  memcpy(uwi->recipe, rs, size);
  b->latest = u;
  b->count++;
  hpcrun_stats_num_unwind_intervals_total_inc();
  return 0;
}


btuwi_status_t
libunw_build_intervals(char *beg_insn, unsigned int len)
{
  unw_context_t uc;
  (void) unw_getcontext(&uc);
  unw_cursor_t c;
  unw_init_local2(&c, &uc, UNW_INIT_SIGNAL_FRAME);
  unw_set_reg(&c, UNW_REG_IP, (intptr_t)beg_insn);
  void *space[2] __attribute((aligned (32))); // enough space for any binarytree
  bitree_uwi_t *dummy = (bitree_uwi_t*)space;
  struct builder b = {DWARF_UNWINDER, dummy, 0};
  int status = unw_reg_states_iterate(&c, dwarf_reg_states_callback, &b);

  intptr_t end = bitree_uwi_rootval(b.latest)->interval.end;
  uintptr_t end_insn = (uintptr_t)beg_insn + len;
  while(status == 0 &&
        (end = bitree_uwi_rootval(b.latest)->interval.end) < end_insn) {
    // If we aren't lined up with the end of the interval, scan for any other
    // info we can possibly pull.
    end++;
    do {
      unw_set_reg(&c, UNW_REG_IP, end);
      status = unw_reg_states_iterate(&c, dwarf_reg_states_callback, &b);
      end++;  // In case this doesn't work, move to the next byte
    } while(status < 0 && end < end_insn);
    if(end >= end_insn) break;  // Scanned to the end, just go with what we got
  }
  bitree_uwi_set_rightsubtree(b.latest, NULL);

  btuwi_status_t stat;
  stat.first_undecoded_ins = NULL;
  stat.count = b.count;
  stat.error = status;
  stat.first = bitree_uwi_rightsubtree(dummy);

  return stat; 
}

// ----------------------------------------------------------
// libunw_unw_step: 
//   Given a cursor, step the cursor to the next (less deeply
//   nested) frame using a recipe found previously, and find
//   a new recipe for the next call.
// ---------------------------------------------------------

step_state
libunw_unw_step(hpcrun_unw_cursor_t* cursor, int *steps_taken)
{
  step_state result = libunw_take_step(cursor);
  if (result != STEP_ERROR) {
    libunw_finalize_cursor(cursor, *steps_taken > 0);
  }

#if DEBUG_LIBUNWIND_INTERFACE
  if (libunwind_debug) {
    printf("result = %s\n", step_state_string(result));
  }
#endif

  return result;
}

void
libunw_uw_recipe_tostr(void *uwr, char str[])
{
  snprintf(str, MAX_RECIPE_STR, "*");
}
