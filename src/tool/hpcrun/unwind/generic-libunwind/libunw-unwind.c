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
#include <unwind/common/unw-datatypes.h>
#include <unwind/common/unwind.h>
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

extern void hpcrun_set_real_siglongjmp(void);

void
hpcrun_unw_init(void)
{
  hpcrun_set_real_siglongjmp();
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
hpcrun_unw_init_cursor(hpcrun_unw_cursor_t* h_cursor, void* context)
{
  unw_cursor_t *cursor = &(h_cursor->uc);
  unw_context_t *ctx = (unw_context_t *) context;
  unw_word_t pc;

  if (ctx != NULL && unw_init_local_signal(cursor, ctx) == 0) {
    unw_get_reg(cursor, UNW_REG_IP, &pc);
  } else {
    pc = 0;
  }

  h_cursor->sp = NULL;
  h_cursor->bp = NULL;
  h_cursor->pc_unnorm = (void *) pc;
  h_cursor->intvl = &(h_cursor->real_intvl);
  h_cursor->real_intvl.lm = NULL;

  compute_normalized_ips(h_cursor);

  TMSG(UNW, "init cursor pc = %p\n", h_cursor->pc_unnorm);
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
hpcrun_unw_step(hpcrun_unw_cursor_t* h_cursor)
{
  // this should never be NULL
  if (h_cursor == NULL) {
    TMSG(UNW, "unw_step: internal error: cursor ptr is NULL\n");
    return STEP_ERROR;
  }

  // starting pc
  unw_cursor_t *cursor = &(h_cursor->uc);
  unw_word_t tmp;
  void *pc;

  unw_get_reg(cursor, UNW_REG_IP, &tmp);
  pc = (void *) tmp;

  // full unwind: stop at libmonitor fence.  this is where we hope the
  // unwind stops.
  if (monitor_unwind_process_bottom_frame(pc)
      || monitor_unwind_thread_bottom_frame(pc))
  {
    TMSG(UNW, "unw_step: stop at monitor fence: %p\n", pc);
    return STEP_STOP;
  }

  int ret = unw_step(cursor);

  // step error, almost never happens with libunwind.
  if (ret < 0) {
    TMSG(UNW, "unw_step: error: unw_step failed at: %p\n", pc);
    return STEP_ERROR;
  }

  // we prefer to stop at monitor fence, but must also stop here.
  if (ret == 0) {
    TMSG(UNW, "unw_step: stop at unw_step: %p\n", pc);
    return STEP_STOP;
  }

  // update for new pc
  unw_get_reg(cursor, UNW_REG_IP, &tmp);
  pc = (void *) tmp;

  h_cursor->sp = NULL;
  h_cursor->bp = NULL;
  h_cursor->pc_unnorm = pc;
  h_cursor->intvl = &(h_cursor->real_intvl);
  h_cursor->real_intvl.lm = NULL;

  compute_normalized_ips(h_cursor);

  TMSG(UNW, "unw_step: advance pc: %p\n", pc);
  return STEP_OK;
}
