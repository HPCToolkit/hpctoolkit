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
// Copyright ((c)) 2002-2016, Rice University
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

//***************************************************************************
//
// HPCToolkit's PPC64 Unwinder
//
//***************************************************************************


//************************* System Include Files ****************************

#include <stddef.h>
#include <stdbool.h>
#include <ucontext.h>
#include <assert.h>


//************************ libmonitor Include Files *************************

#include <monitor.h>


//*************************** User Include Files ****************************

#include <unwind/common/unwind.h>
#include <unwind/common/unw-datatypes.h>

#include "ppc64-unwind-interval.h"

#include "sample_event.h"

#include "splay.h"
#include "ui_tree.h"

#include <messages/messages.h>

#include <utilities/ip-normalized.h>
#include <utilities/arch/mcontext.h>

#include <lib/isa-lean/power/instruction-set.h>
#include <unwind/common/fence_enum.h>

//***************************************************************************
// forward declarations
//***************************************************************************

#define MYDBG 0

typedef enum {
  UnwFlg_NULL = 0,
  UnwFlg_StackTop,
} unw_flag_t;

static fence_enum_t
hpcrun_check_fence(void* ip);

//***************************************************************************
// interface functions
//***************************************************************************

static bool
fence_stop(fence_enum_t fence)
{
  return (fence == FENCE_MAIN) || (fence == FENCE_THREAD);
}

extern void hpcrun_set_real_siglongjmp(void);

void
hpcrun_unw_init(void)
{
  hpcrun_interval_tree_init();
  hpcrun_set_real_siglongjmp();
}

//
// register codes (only 1 at the moment)
//
typedef enum {
  UNW_REG_IP
} unw_reg_code_t;


static int 
hpcrun_unw_get_unnorm_reg(hpcrun_unw_cursor_t* cursor, unw_reg_code_t reg_id,
			  void** reg_value)
{
  assert(reg_id == UNW_REG_IP);
  *reg_value = cursor->pc_unnorm;
  
  return 0;
}


static int 
hpcrun_unw_get_norm_reg(hpcrun_unw_cursor_t* cursor, unw_reg_code_t reg_id,
			ip_normalized_t* reg_value)
{
  assert(reg_id == UNW_REG_IP);
  *reg_value = cursor->pc_norm;
  
  return 0;
}


int
hpcrun_unw_get_ip_norm_reg(hpcrun_unw_cursor_t* c, ip_normalized_t* reg_value)
{
  return hpcrun_unw_get_norm_reg(c, UNW_REG_IP, reg_value);
}


int
hpcrun_unw_get_ip_unnorm_reg(hpcrun_unw_cursor_t* c, unw_word_t* reg_value)
{
  return hpcrun_unw_get_unnorm_reg(c, UNW_REG_IP, reg_value);
}


//
// unimplemented for now
//  fix when trampoline support is added
//
void*
hpcrun_unw_get_ra_loc(hpcrun_unw_cursor_t* cursor)
{
  return NULL;
}


void 
hpcrun_unw_init_cursor(hpcrun_unw_cursor_t* cursor, void* context)
{
  ucontext_t* ctxt = (ucontext_t*)context;

  cursor->pc_unnorm = ucontext_pc(ctxt);
  cursor->pc_norm   = (ip_normalized_t) ip_normalized_NULL;
  cursor->ra        = NULL;
  cursor->sp        = ucontext_sp(ctxt);
  cursor->bp        = NULL;
  cursor->flags     = UnwFlg_StackTop;

  // Capture unnormalized ip here b/c hpcrun_addr_to_interval() will
  // not call hpcrun_normalize_ip() in exceptional cases.
  cursor->pc_norm = ((ip_normalized_t)
                     { .lm_id = HPCRUN_FMT_LMId_NULL,
		       .lm_ip = (uintptr_t) cursor->pc_unnorm });

  unw_interval_t* intvl = (unw_interval_t*)
    hpcrun_addr_to_interval(cursor->pc_unnorm,
			    cursor->pc_unnorm, &cursor->pc_norm);

  cursor->intvl = (splay_interval_t*)intvl;
  
  if (intvl && intvl->ra_ty == RATy_Reg) {
    if (intvl->ra_arg == PPC_REG_LR) {
      cursor->ra = (void*)(ctxt->uc_mcontext.regs->link);
    }
    else {
      cursor->ra = (void*)(ctxt->uc_mcontext.regs->gpr[intvl->ra_arg]);
    }
  }
  
  TMSG(UNW, "init: pc=%p, ra=%p, sp=%p, fp=%p", 
       cursor->pc_unnorm, cursor->ra, cursor->sp, cursor->bp);
  if (MYDBG) { ui_dump(intvl); }
}


// --FIXME--: add advanced fence processing and enclosing function to cursor here
//

int 
hpcrun_unw_step(hpcrun_unw_cursor_t* cursor)
{

  // current frame
  void*  pc = cursor->pc_unnorm;
  void** sp = cursor->sp;
  void** fp = cursor->bp; // unused
  unw_interval_t* intvl = (unw_interval_t*)(cursor->intvl);

  bool isInteriorFrm = (cursor->flags != UnwFlg_StackTop);
  
  // next (parent) frame
  void*           nxt_pc = NULL;
  ip_normalized_t nxt_pc_norm = ip_normalized_NULL;
  void** nxt_sp = NULL;
  void** nxt_fp = NULL; // unused
  void*  nxt_ra = NULL; // always NULL unless we go through a signal handler
  unw_interval_t* nxt_intvl = NULL;
  
  if (!intvl) {
    TMSG(UNW, "error: missing interval for pc=%p", pc);
    return STEP_ERROR;
  }

  
  cursor->fence = hpcrun_check_fence(cursor->pc_unnorm);

#if 0
  //-----------------------------------------------------------
  // check for outermost frame (return STEP_STOP only after outermost
  // frame has been identified as valid)
  //-----------------------------------------------------------
  if (monitor_in_start_func_wide(pc)) {
    TMSG(UNW, "stop: monitor_in_start_func_wide, pc=%p", pc);
    return STEP_STOP;
  }
#endif  
  //-----------------------------------------------------------
  // check if we have reached the end of our unwind, which is
  // demarcated with a fence. 
  //-----------------------------------------------------------
  if (fence_stop(cursor->fence)) {
    TMSG(UNW,"unw_step: STEP_STOP, current pc in monitor fence pc=%p\n", cursor->pc_unnorm);
    return STEP_STOP;
  }

  if ((void*)sp >= monitor_stack_bottom()) {
    TMSG(FENCE_UNW, "stop: sp (%p) >= unw_stack_bottom", sp);
    cursor->fence = FENCE_MAIN;
    return STEP_STOP;
  }

  //-----------------------------------------------------------
  // compute SP (stack pointer) for the caller's frame.  Do this first
  //   because we rely on the invariant that an interior frame contains
  //   a stack pointer and, above the stack pointer, a return address.
  //-----------------------------------------------------------
  if (intvl->sp_ty == SPTy_Reg) {
    // SP already points to caller's stack frame
    nxt_sp = sp;
    
    // consistency check: interior frames should not have type SPTy_Reg
    if (isInteriorFrm) {
      nxt_sp = *sp;
      TMSG(UNW, "warning: correcting sp: %p -> %p", sp, nxt_sp);
    }
  }
  else if (intvl->sp_ty == SPTy_SPRel) {
    // SP points to parent's SP
    nxt_sp = *sp;
  }
  else {
    // assert(0);
    EMSG("unwind failure computing SP at pc: %p, sp: %p", pc, sp);
    return STEP_ERROR;
  }


  //-----------------------------------------------------------
  // compute RA (return address) for the caller's frame
  //-----------------------------------------------------------
  if (intvl->ra_ty == RATy_Reg) {
    nxt_pc = cursor->ra;

    // consistency check: interior frames should not have type RATy_Reg
    if (isInteriorFrm) {
      nxt_pc = getNxtPCFromSP(nxt_sp);
      TMSG(UNW, "warning: correcting pc: %p -> %p", cursor->ra, nxt_pc);
    }
  }
  else if (intvl->ra_ty == RATy_SPRel) {
    nxt_pc = getNxtPCFromSP(nxt_sp);
  }
  else {
    // assert(0);
    EMSG("unwind failure computing RA at pc: %p, sp: %p", pc, sp);
    return STEP_ERROR;
  }


  //-----------------------------------------------------------
  // compute unwind information for the caller's pc
  //-----------------------------------------------------------
  nxt_intvl = (unw_interval_t*)hpcrun_addr_to_interval(nxt_pc,
						       nxt_pc, &nxt_pc_norm);
 
  // if nxt_pc is invalid for some reason...
  if (!nxt_intvl) {
    TMSG(UNW, "warning: bad nxt pc=%p; sp=%p, fp=%p...", nxt_pc, sp, fp);

    //-------------------------------------------------------------------
    // If this is a leaf frame, assume the interval didn't correctly
    // track the return address.  Try one frame deeper.
    //-------------------------------------------------------------------
    void** try_sp = NULL;
    if (!isInteriorFrm) {
      try_sp = *nxt_sp;

      // Sanity check SP: Once in a while SP is clobbered.
      if (isPossibleParentSP(nxt_sp, try_sp)) {
	nxt_pc = getNxtPCFromSP(try_sp);
	nxt_intvl = (unw_interval_t*)hpcrun_addr_to_interval(nxt_pc, nxt_pc,
							     &nxt_pc_norm);
      }
    }
     
    if (!nxt_intvl) {
      TMSG(UNW, "error: skip-frame failed: nxt pc=%p, sp=%p; try sp=%p", 
	   nxt_pc, nxt_sp, try_sp);
      return STEP_ERROR;
    }

    // INVARIANT: 'try_sp' is valid
    nxt_sp = try_sp;
    TMSG(UNW, "skip-frame: nxt pc=%p, sp=%p", nxt_pc, nxt_sp);
  }
  // INVARIANT: At this point, 'nxt_intvl' is valid


  // INVARIANT: Ensure we always make progress unwinding the stack...
  bool mayFrameSizeBe0 = (intvl->sp_ty == SPTy_Reg && !isInteriorFrm);
  if (!mayFrameSizeBe0 && !isPossibleParentSP(sp, nxt_sp)) {
    // TMSG(UNW, " warning: adjust sp b/c nxt_sp=%p < sp=%p", nxt_sp, sp);
    // nxt_sp = sp + 1
    TMSG(UNW, "error: loop! nxt_sp=%p, sp=%p", nxt_sp, sp);
    return STEP_ERROR;
  }


  TMSG(UNW, "next: pc=%p, sp=%p, fp=%p", nxt_pc, nxt_sp, nxt_fp);
  if (MYDBG) { ui_dump(nxt_intvl); }

  cursor->pc_unnorm = nxt_pc;
  cursor->pc_norm   = nxt_pc_norm;
  cursor->ra        = nxt_ra;
  cursor->sp        = nxt_sp;
  cursor->bp        = nxt_fp;
  cursor->intvl     = (splay_interval_t*)nxt_intvl;
  cursor->flags     = UnwFlg_NULL;

  return STEP_OK;
}

static fence_enum_t
hpcrun_check_fence(void* ip)
{
  fence_enum_t rv = FENCE_NONE;
  if (monitor_unwind_process_bottom_frame(ip))
    rv = FENCE_MAIN;
  else if (monitor_unwind_thread_bottom_frame(ip))
    rv = FENCE_THREAD;

   if (ENABLED(FENCE_UNW) && rv != FENCE_NONE)
     TMSG(FENCE_UNW, "%s", fence_enum_name(rv));
   return rv;
}
