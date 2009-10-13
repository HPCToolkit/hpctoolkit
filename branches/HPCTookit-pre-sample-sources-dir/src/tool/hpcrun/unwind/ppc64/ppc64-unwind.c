// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

#include "ppc64-unwind-interval.h"

#include "unwind.h"
#include "unwind_cursor.h"

#include "splay.h"
#include "ui_tree.h"

#include <messages/messages.h>

// FIXME: see note in mips-unwind.c
#include <lib/isa/instructionSets/ppc.h>


//***************************************************************************
// forward declarations
//***************************************************************************

#define MYDBG 0

typedef enum {
  UnwFlg_NULL = 0,
  UnwFlg_StackTop,
} unw_flag_t;


//***************************************************************************
// private operations
//***************************************************************************

#if __WORDSIZE == 32
#  define UCONTEXT_REG(uc, reg) ((uc)->uc_mcontext.uc_regs->gregs[reg])
#else
#  define UCONTEXT_REG(uc, reg) ((uc)->uc_mcontext.gp_regs[reg])
#endif

static inline void *
ucontext_pc(ucontext_t *context)
{
  return (void **)UCONTEXT_REG(context, PPC_REG_PC);
}


static inline void **
ucontext_fp(ucontext_t *context)
{
  return (void **)UCONTEXT_REG(context, PPC_REG_FP);
}


static inline void **
ucontext_sp(ucontext_t *context)
{
  return (void **)UCONTEXT_REG(context, PPC_REG_SP);
}


//***************************************************************************

static inline void*
getNxtPCFromSP(void** sp)
{
#ifdef __PPC64__
  static const int RA_OFFSET_FROM_SP = 2;
#else
  static const int RA_OFFSET_FROM_SP = 1;
#endif
  return *(sp + RA_OFFSET_FROM_SP);
}


static inline bool
isPossibleParentSP(void** sp, void** parent_sp)
{
  // Stacks grow down, so outer frames are at higher addresses
  return (parent_sp > sp); // assume frame size is not 0
}


//***************************************************************************
// interface functions
//***************************************************************************

void *
context_pc(void *context)
{
  ucontext_t* ctxt = (ucontext_t*)context;
  return ucontext_pc(ctxt);
}


void
unw_init(void)
{
  hpcrun_interval_tree_init();
}


int 
unw_get_reg(unw_cursor_t *cursor, unw_reg_code_t reg_id, void **reg_value)
{
  assert(reg_id == UNW_REG_IP);
  *reg_value = cursor->pc;
  return 0;
}

//
// unimplemented for now
//  fix when trampoline support is added
//

void*
hpcrun_unw_get_ra_loc(unw_cursor_t* cursor)
{
  return NULL;
}

void 
unw_init_cursor(unw_cursor_t *cursor, void *context)
{
  ucontext_t* ctxt = (ucontext_t*)context;

  cursor->pc = ucontext_pc(ctxt);
  cursor->ra = NULL;
  cursor->sp = ucontext_sp(ctxt);
  cursor->bp = NULL;
  cursor->flags = UnwFlg_StackTop;

  unw_interval_t* intvl = (unw_interval_t*)hpcrun_addr_to_interval(cursor->pc);
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
       cursor->pc, cursor->ra, cursor->sp, cursor->bp);
  if (MYDBG) { ui_dump(intvl); }
}


int 
unw_step(unw_cursor_t *cursor)
{
  // current frame
  void*  pc = cursor->pc;
  void** sp = cursor->sp;
  void** fp = cursor->bp; // unused
  unw_interval_t* intvl = (unw_interval_t*)(cursor->intvl);

  bool isInteriorFrm = (cursor->flags != UnwFlg_StackTop);
  
  // next (parent) frame
  void*  nxt_pc = NULL;
  void** nxt_sp = NULL;
  void** nxt_fp = NULL; // unused
  void*  nxt_ra = NULL; // always NULL unless we go through a signal handler
  unw_interval_t* nxt_intvl = NULL;
  
  if (!intvl) {
    TMSG(UNW, "error: missing interval for pc=%p", pc);
    return STEP_ERROR;
  }

  
  //-----------------------------------------------------------
  // check for outermost frame (return STEP_STOP only after outermost
  // frame has been identified as valid)
  //-----------------------------------------------------------
  if (monitor_in_start_func_wide(pc)) {
    TMSG(UNW, "stop: monitor_in_start_func_wide, pc=%p", pc);
    return STEP_STOP;
  }
  
  if ((void*)sp >= monitor_stack_bottom()) {
    TMSG(UNW, "stop: sp (%p) >= unw_stack_bottom", sp);
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
    assert(0);
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
    assert(0);
  }


  //-----------------------------------------------------------
  // compute unwind information for the caller's pc
  //-----------------------------------------------------------
  nxt_intvl = (unw_interval_t*)hpcrun_addr_to_interval(nxt_pc);

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
	nxt_intvl = (unw_interval_t*)hpcrun_addr_to_interval(nxt_pc);
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

  cursor->pc = nxt_pc;
  cursor->ra = nxt_ra;
  cursor->sp = nxt_sp;
  cursor->bp = nxt_fp;
  cursor->intvl = (splay_interval_t*)nxt_intvl;
  cursor->flags = UnwFlg_NULL;

  return STEP_OK;
}

