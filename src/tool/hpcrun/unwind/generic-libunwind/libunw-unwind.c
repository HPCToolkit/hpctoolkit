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
// Copyright ((c)) 2002-2017, Rice University
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
//
// File:
//   libunw-unwind.c
//
// Purpose:
//    Implement the hpcrun unwind primitives using libunwind
//
//        hpcrun_unw_init
//        hpcrun_unw_init_cursor
//        hpcrun_unw_get_ra_loc
//        hpcrun_unw_get_ip_reg
//        hpcrun_unw_step
//        hpcrun_unw_get_ip_unnorm_reg
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
#include <libunwind.h>

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
// private functions
//************************************************

static void
compute_normalized_ips(hpcrun_unw_cursor_t* cursor)
{
  void *func_start_pc = NULL;
  void *func_end_pc = NULL;
  load_module_t* lm = NULL;

  fnbounds_enclosing_addr(cursor->pc_unnorm, &func_start_pc, &func_end_pc, &lm);

  cursor->pc_norm = hpcrun_normalize_ip(cursor->pc_unnorm, lm);
  cursor->the_function = hpcrun_normalize_ip(func_start_pc, lm);
}

//************************************************
// interface functions
//************************************************

// ----------------------------------------------------------
// hpcrun_unw_init
// ----------------------------------------------------------

void
hpcrun_unw_init(void)
{
  uw_recipe_map_init();
}


// ----------------------------------------------------------
// hpcrun_unw_get_ip_reg
// ----------------------------------------------------------

int
hpcrun_unw_get_ip_reg(hpcrun_unw_cursor_t* cursor, void** val)
{
  unw_word_t tmp;
  int rv = unw_get_reg(&(cursor->uc), UNW_REG_IP, &tmp);
  *val = (void*) tmp;
  return rv;
}

int
hpcrun_unw_get_ip_unnorm_reg(hpcrun_unw_cursor_t* cursor, void** val)
{
  *val = cursor->pc_unnorm;
  return 0;
}

int
hpcrun_unw_get_ip_norm_reg(hpcrun_unw_cursor_t* cursor, ip_normalized_t* val)
{
  *val = cursor->pc_norm;
  return 0;
}

// ----------------------------------------------------------
// hpcrun_unw_get_ra_loc (no trampolines yet)
// ----------------------------------------------------------

void*
hpcrun_unw_get_ra_loc(hpcrun_unw_cursor_t* c)
{
  return NULL;
}

// ----------------------------------------------------------
// hpcrun_unw_init_cursor
// ----------------------------------------------------------

// Takes a sigaction() context pointer and makes a libunwind
// unw_cursor_t.  The other fields in hpcrun_unw_cursor_t are unused,
// except for pc_norm and pc_unnorm.
//
void
hpcrun_unw_init_cursor(hpcrun_unw_cursor_t* cursor, void* context)
{
  unw_cursor_t *unw_cursor = &(cursor->uc);
  unw_context_t *ctx = (unw_context_t *) context;
  unw_word_t pc;

  if (ctx != NULL && unw_init_local2(unw_cursor, ctx, UNW_INIT_SIGNAL_FRAME) == 0) {
    unw_get_reg(unw_cursor, UNW_REG_IP, &pc);
  } else {
    pc = 0;
  }

  cursor->pc_unnorm = (void *) pc;

  compute_normalized_ips(cursor);

  TMSG(UNW, "init cursor pc = %p\n", cursor->pc_unnorm);
}


// ----------------------------------------------------------
// hpcrun_unw_step: 
//   Given a cursor, step the cursor to the next (less deeply
//   nested) frame.  Conforms to the semantics of libunwind's
//   hpcrun_unw_step.  In particular, returns:
//     > 0 : successfully advanced cursor to next frame
//       0 : previous frame was the end of the unwind
//     < 0 : error condition
// ---------------------------------------------------------

step_state
hpcrun_unw_step(hpcrun_unw_cursor_t* cursor)
{
  // this should never be NULL
  if (cursor == NULL) {
    TMSG(UNW, "unw_step: internal error: cursor ptr is NULL\n");
    return STEP_ERROR;
  }

  // starting pc
  unw_cursor_t *unw_cursor = &(cursor->uc);
  unw_word_t tmp;
  void *pc;

  unw_get_reg(unw_cursor, UNW_REG_IP, &tmp);
  pc = (void *) tmp;

  // full unwind: stop at libmonitor fence.  this is where we hope the
  // unwind stops.
  cursor->fence = (monitor_unwind_process_bottom_frame(pc) ? FENCE_MAIN :
		   monitor_unwind_thread_bottom_frame(pc)? FENCE_THREAD : FENCE_NONE);
  if (cursor->fence != FENCE_NONE)
    {
      TMSG(UNW, "unw_step: stop at monitor fence: %p\n", pc);
      return STEP_STOP;
    }

  //-----------------------------------------------------------
  // compute unwind information for the caller's pc
  //-----------------------------------------------------------
  unwindr_info_t unwr_info;
  bool found = uw_recipe_map_lookup(pc, &unwr_info);
  if (!found)
    {
      TMSG(UNW, "unw_step: error: unw_step failed at: %p\n", pc);
      return STEP_ERROR;
    }

  bitree_uwi_t* uw = unwr_info.btuwi;
  uwi_t *uwi = bitree_uwi_rootval(uw);
  unw_apply_reg_state(unw_cursor, uwi->recipe);

  // update for new pc
  unw_get_reg(unw_cursor, UNW_REG_IP, &tmp);
  pc = (void *) tmp;

  cursor->pc_unnorm = pc;

  compute_normalized_ips(cursor);

  TMSG(UNW, "unw_step: advance pc: %p\n", pc);
  return STEP_OK;
}

//***************************************************************************
// unwind_interval interface
//***************************************************************************

struct builder
{
  mem_alloc m_alloc;
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
  bitree_uwi_t *u = bitree_uwi_malloc(b->m_alloc, size);
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


static btuwi_status_t
libunw_build_intervals(char *beg_insn, unsigned int len, mem_alloc m_alloc)
{
  unw_context_t uc;
  unw_getcontext(&uc);
  unw_cursor_t c;
  unw_init_local2(&c, &uc, UNW_INIT_SIGNAL_FRAME);
  unw_set_reg(&c, UNW_REG_IP, (intptr_t)beg_insn);
  void *space[2] __attribute((aligned (8))); // enough space for any binarytree
  bitree_uwi_t *dummy = (bitree_uwi_t*)space;
  struct builder b = {m_alloc, dummy, 0};
  int status = unw_reg_states_iterate(&c, dwarf_reg_states_callback, &b);
  /* whatever libutils says about the last address range, we insist
   * that it extend to the last address of this function range.  But
   * we don't trust beg_insn + len to reach the end of the function
   * range, necessarily.  */
  if (bitree_uwi_rootval(b.latest)->interval.end < (uintptr_t)(beg_insn + len))
    bitree_uwi_rootval(b.latest)->interval.end = (uintptr_t)(beg_insn + len);
  bitree_uwi_set_rightsubtree(b.latest, NULL);

  btuwi_status_t stat;
  stat.first_undecoded_ins = NULL;
  stat.count = b.count;
  stat.errcode = status;
  stat.first = bitree_uwi_rightsubtree(dummy);

  return stat; 
}

btuwi_status_t
build_intervals(char  *ins, unsigned int len, mem_alloc m_alloc)
{
  btuwi_status_t stat = libunw_build_intervals(ins, len, m_alloc);
  return stat;
}

