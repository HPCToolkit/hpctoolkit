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
// HPCToolkit's MIPS Unwinder
// 
// Nathan Tallent
// Rice University
//
// When part of HPCToolkit, this code assumes HPCToolkit's license;
// see www.hpctoolkit.org.
//
//***************************************************************************

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <ucontext.h>


//************************ libmonitor Include Files *************************

// see mips-unwind-cfg.h

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "unwind.h"

#include "mips-unwind-cfg.h"
#include "mips-unwind-interval.h"

#include "stack_troll.h"
#include "sample_event.h"

#include <lib/isa-lean/mips/instruction-set.h>


//*************************** Forward Declarations **************************

#define MYDBG 0

#define pc_NULL ((void*)(intptr_t)(-1)) /* NULL may identify outmost frame */


//***************************************************************************
// quasi interface functions
//***************************************************************************

static inline bool 
unw_isin_start_func(void* pc)
{
#if (HPC_UNW_LITE)
  return dylib_isin_start_func(pc);
#else
  return (bool)monitor_in_start_func_wide(pc);
#endif
}


static inline void**
unw_stack_bottom()
{
#if (HPC_UNW_LITE)
  // Very nice glibc symbol.  Note also that 'environ' works since the
  // environment is just above the stack bottom.
  extern void** __libc_stack_end;
  return __libc_stack_end; // FIXME: thread stack bottom?
#else
  return (void**)monitor_stack_bottom();
#endif
}


//***************************************************************************
// private operations
//***************************************************************************

// NOTE: for the n32 ABI, the registers are technically 64 bits, even
// though pointers are only 32 bits.

static inline greg_t
ucontext_getreg(ucontext_t* context, int reg)
{ return context->uc_mcontext.gregs[reg]; }


static inline void*
ucontext_pc(ucontext_t* context)
{ return (void*)(uintptr_t)context->uc_mcontext.pc; }


static inline void*
ucontext_ra(ucontext_t* context)
{ return (void*)(uintptr_t)ucontext_getreg(context, MIPS_REG_RA); }


static inline void**
ucontext_sp(ucontext_t* context)
{ return (void**)(uintptr_t)ucontext_getreg(context, MIPS_REG_SP); }


static inline void**
ucontext_fp(ucontext_t* context)
{ return (void**)(uintptr_t)ucontext_getreg(context, MIPS_REG_FP); }


//***************************************************************************

static inline void*
getNxtPCFromRA(void* ra)
{
  // both the call instruction and the delay slot have effects
  //(uintptr_t)ra - 4 /*delay slot*/ - 4 /*next insn*/;
  return ra;
}


static inline void**
getPtrFromPtr(void** ptr, int offset)
{ return (void**)((uintptr_t)ptr + offset); }


static inline void*
getValFromPtr(void** ptr, int offset)
{ return *getPtrFromPtr(ptr, offset); }


static inline bool
isPossibleParentSP(void** sp, void** parent_sp)
{
  // Stacks grow down, so outer frames are at higher addresses
  return (parent_sp > sp); // assume frame size is not 0
}


static inline bool
isPossibleFP(void** sp, void** fp)
{
  // Stacks grow down, so frame pointer is at a higher address
  return (unw_stack_bottom() >= fp && fp >= sp);
}


//***************************************************************************

static inline void
computeNext_SPFrame(void** * nxt_sp, void** * nxt_fp,
		    unw_interval_t* intvl, void** sp, void** fp)
{
  // intvl->sp_arg != unwarg_NULL
  *nxt_sp = getPtrFromPtr(sp, intvl->sp_arg);

  if (intvl->fp_arg != unwarg_NULL) {
    *nxt_fp = getValFromPtr(sp, intvl->fp_arg);
  }
  else if (fp) {
    // preserve FP for use with parent frame (child may save parent's FP)
    *nxt_fp = fp;
  }
}


static inline void
computeNext_FPFrame(void** * nxt_sp, void** * nxt_fp,
		    unw_interval_t* intvl, void** fp)
{
  if (intvl->sp_arg != unwarg_NULL) {
    if (frameflg_isset(intvl->flgs, FrmFlg_FPOfstPos)) {
      *nxt_sp = getPtrFromPtr(fp, intvl->sp_arg);
    }
    else {
      *nxt_sp = getValFromPtr(fp, intvl->sp_arg);
    }
  }
  else {
    *nxt_sp = fp;
  }
  
  if (intvl->fp_arg != unwarg_NULL) {
    *nxt_fp = getValFromPtr(fp, intvl->fp_arg);
  }
}


//***************************************************************************

static inline bool
isPossibleSignalTrampoline(void* ra, void** sp)
{
  return (unw_stack_bottom() > (void**)ra && (void**)ra > sp);
}


static inline bool
isSignalTrampoline(void* addr)
{
  // Signal trampolines for MIPS n64 and n32 (respectively):
  // addr:  li      v0,5211  |  addr: li      v0,6211
  //        syscall          |        syscall

#define MIPS_SIGTRAMP_LI_VO_N64 (0x24020000 + 5000 + 211)
#define MIPS_SIGTRAMP_LI_VO_N32 (0x24020000 + 6000 + 211)

  uint32_t* insn1 = (uint32_t*)addr;
  uint32_t* insn2 = insn1 + 1;

  return ( (*insn1 == MIPS_SIGTRAMP_LI_VO_N64 || 
	    *insn1 == MIPS_SIGTRAMP_LI_VO_N32)
	   && (*insn2 == MIPS_OP_SYSCALL) );
}


static inline ucontext_t*
getSignalContextFromTrampline(void* sigtramp)
{
  // Signal context is a fixed distance from first instruction in the
  // signal trampoline
#define MIPS_SIGFRAME_CODE_OFST         (4 * 4)
#define MIPS_SIGFRAME_SIGCONTEXT_OFST   (6 * 4)
#define MIPS_RTSIGFRAME_SIGINFO_SZ      128

  int ofst = - MIPS_SIGFRAME_CODE_OFST + (MIPS_SIGFRAME_SIGCONTEXT_OFST
					  + MIPS_RTSIGFRAME_SIGINFO_SZ);
  ucontext_t* ctxt = (ucontext_t*)getPtrFromPtr(sigtramp, ofst);
  return ctxt;
}


//***************************************************************************

static inline bool
isAfterCall(void* addr)
{
  uint32_t* ra = (uint32_t*)addr;
  uint32_t insn = *(ra - 1/*delay slot*/ - 1/*potential call*/);

  // jalr rs [rd = 31 [RA] is implied]
  uint32_t op_spc = (insn & MIPS_OPClass_Special_MASK);
  return (op_spc == MIPS_OP_JALR && (MIPS_OPND_REG_D(insn) == MIPS_REG_RA));
}


static validation_status
validateTroll(void* addr, void* arg)
{
  bool isInCode = false;

#if (HPC_UNW_LITE)
  void *proc_beg = NULL, *mod_beg = NULL;
  dylib_find_proc(addr, &proc_beg, &mod_beg);
  isInCode = (mod_beg || proc_beg);
#else
  void* proc_beg = NULL;
  void* proc_end = NULL;
  bool ret = fnbounds_enclosing_addr(addr, &proc_beg, &proc_end, NULL);
  isInCode = ret && proc_beg;
#endif
  
  if (isInCode && isAfterCall(addr)) {
    return UNW_ADDR_CONFIRMED;
  }
  else {
    return UNW_ADDR_WRONG;
  }
}


//***************************************************************************
// interface functions
//***************************************************************************

void*
hpcrun_context_pc(void* context)
{ 
  ucontext_t* ctxt = context;
  return ucontext_pc(ctxt);
}

extern void hpcrun_set_real_siglongjmp(void);

void
hpcrun_unw_init(void)
{
  HPC_IFNO_UNW_LITE(hpcrun_interval_tree_init(););
  hpcrun_set_real_siglongjmp();
}


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
hpcrun_unw_get_ip_unnorm_reg(hpcrun_unw_cursor_t* c, void** reg_value)
{
  return hpcrun_unw_get_unnorm_reg(c, UNW_REG_IP, reg_value);
}

// unimplemented for now
//  fix when trampoline support is added
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
  cursor->ra = ucontext_ra(ctxt);
  cursor->sp = ucontext_sp(ctxt);
  cursor->bp = ucontext_fp(ctxt);

  UNW_INTERVAL_t intvl = demand_interval(cursor->pc_unnorm, true/*isTopFrame*/);
  cursor->intvl = CASTTO_UNW_CURSOR_INTERVAL_t(intvl);
  cursor->pc_norm = hpcrun_normalize_ip(cursor->pc_unnorm, UI_FLD(intvl,lm));
  
  if (!UI_IS_NULL(intvl)) {
    if (frameflg_isset(UI_FLD(intvl,flgs), FrmFlg_RAReg)) {
      cursor->ra = 
	(void*)(uintptr_t)ucontext_getreg(context, UI_FLD(intvl, ra_arg));
    }
    if (frameflg_isset(UI_FLD(intvl,flgs), FrmFlg_FPInV)) {
      // FIXME: it would be nice to preserve the parent FP
    }
  }

  TMSG(UNW, "init: pc=%p, pc_norm: id= %d offset=%p ra=%p, sp=%p, fp=%p", 
       cursor->pc_unnorm, cursor->pc_norm->id, cursor->pc_norm->offset,
       idcursor->ra, cursor->sp, cursor->bp);
  if (MYDBG) { ui_dump(UI_ARG(intvl)); }
}

// --FIXME--: add advanced fence processing and enclosing function to cursor here
//

oint 
hpcrun_unw_step(hpcrun_unw_cursor_t* cursor)
{
  // current frame:
  void*  pc = cursor->pc_unnorm;
  void** sp = cursor->sp;
  void** fp = cursor->bp;
  UNW_INTERVAL_t intvl = CASTTO_UNW_INTERVAL_t(cursor->intvl);

  // next (parent) frame
  void*           nxt_pc = pc_NULL;
  ip_normalized_t nxt_pc_norm = ip_normalized_NULL;
  void** nxt_sp = NULL;
  void** nxt_fp = NULL;
  void*  nxt_ra = NULL; // always NULL unless we go through a signal handler
  UNW_INTERVAL_t nxt_intvl = UNW_INTERVAL_NULL;

  if (UI_IS_NULL(intvl)) {
    TMSG(UNW, "error: missing interval for pc=%p", pc);
    return STEP_ERROR;
  }
  

  //-----------------------------------------------------------
  // check for outermost frame (return STEP_STOP only after outermost
  // frame has been identified as valid)
  //-----------------------------------------------------------
  if (unw_isin_start_func(pc)) {
    TMSG(UNW, "stop: unw_isin_start_func, pc=%p", pc);
    return STEP_STOP;
  }

  if (sp >= unw_stack_bottom()) {
    TMSG(UNW, "stop: sp (%p) >= unw_stack_bottom", sp);
    return STEP_STOP;
  }

  
  //-----------------------------------------------------------
  // compute RA (return address) for the caller's frame
  //-----------------------------------------------------------
  if (frameflg_isset(UI_FLD(intvl,flgs), FrmFlg_RAReg)) {
    // RA should be in a register only on the first call to unw_step()
    // (or the first call after going through a signal trampoline)
    nxt_pc = cursor->ra;
  }
  else if (UI_FLD(intvl,ty) == FrmTy_SP) {
    nxt_pc = getValFromPtr(sp, UI_FLD(intvl,ra_arg));
  }
  else if (UI_FLD(intvl,ty) == FrmTy_FP) {
    if (isPossibleFP(sp, fp)) { // FP is our weak spot
      nxt_pc = getValFromPtr(fp, UI_FLD(intvl,ra_arg));
    }
  }
  else {
    assert(0);
  }
  // INVARIANT: nxt_pc has been overwritten or is pc_NULL

#if 0
  if (nxt_pc == NULL) {
    // Allegedly, MIPS API promises NULL RA for outermost frame
    // NOTE: we are already past the last valid PC
    TMSG(UNW, "stop: pc=%p", nxt_pc);
    return STEP_STOP;
  }
#endif

  //-----------------------------------------------------------
  // compute SP (stack pointer) and FP (frame pointer) for the caller's frame.
  //-----------------------------------------------------------
  if (UI_FLD(intvl,ty) == FrmTy_SP) {
    computeNext_SPFrame(&nxt_sp, &nxt_fp, UI_ARG(intvl), sp, fp);
  }
  else if (UI_FLD(intvl,ty) == FrmTy_FP) {
    if (isPossibleFP(sp, fp)) { // FP is our weak spot
      computeNext_FPFrame(&nxt_sp, &nxt_fp, UI_ARG(intvl), fp);
    }
  }
  else {
    assert(0);
  }

  //-----------------------------------------------------------
  // compute unwind information for the caller's pc
  //-----------------------------------------------------------  

  // check for a signal handling trampoline
  bool isTopFrame = false;
  if (isPossibleSignalTrampoline(nxt_pc, nxt_sp)
      && isSignalTrampoline(nxt_pc)) {
    ucontext_t* ctxt = getSignalContextFromTrampline(nxt_pc);
    nxt_pc = ucontext_pc(ctxt);
    nxt_ra = ucontext_ra(ctxt);
    nxt_sp = ucontext_sp(ctxt);
    nxt_fp = ucontext_fp(ctxt);
    isTopFrame = true;
    TMSG(UNW, "signal tramp context: pc=%p, sp=%p, fp=%p", 
	 nxt_pc, nxt_sp, nxt_fp);
  }

  // try to obtain interval
  if (nxt_pc != pc_NULL) {
    nxt_intvl = demand_interval(getNxtPCFromRA(nxt_pc), isTopFrame);
    nxt_pc_norm = hpcrun_normalize_ip(nxt_pc, UI_FLD(nxt_intvl,lm));
  }

  // if nxt_pc is invalid for some reason, try trolling
  bool didTroll = false;
  if (UI_IS_NULL(nxt_intvl)) {
    TMSG(UNW, "warning: bad nxt pc=%p; sp=%p, fp=%p...", nxt_pc, sp, fp);

    unsigned int troll_pc_ofst;
    troll_status ret = stack_troll(sp, &troll_pc_ofst, validateTroll, NULL);
    if (ret == TROLL_INVALID) {
      TMSG(UNW, "error: troll failed");
      return STEP_ERROR;
    }
    void** troll_sp = (void**)getPtrFromPtr(sp, troll_pc_ofst);
    
    nxt_intvl = demand_interval(getNxtPCFromRA(*troll_sp), false/*topFrame*/);
    if (UI_IS_NULL(nxt_intvl)) {
      TMSG(UNW, "error: troll_pc=%p failed", *troll_sp);
      return STEP_ERROR;
    }
    else {
      nxt_pc_norm = hpcrun_normalize_ip(nxt_pc, UI_FLD(nxt_intvl,lm));
    }
    didTroll = true;

    nxt_pc = *troll_sp;

    if (!frameflg_isset(UI_FLD(intvl,flgs), FrmFlg_RAReg) 
	&& UI_FLD(intvl,ra_arg) != unwarg_NULL) {
      // realign sp/fp and recompute nxt_sp, nxt_fp
      if (UI_FLD(intvl,ty) == FrmTy_SP) {
	void** new_sp = getPtrFromPtr(troll_sp, -UI_FLD(intvl,ra_arg));
	computeNext_SPFrame(&nxt_sp, &nxt_fp, UI_ARG(intvl), new_sp, NULL);
	TMSG(UNW, "troll align/sp: troll_sp=%p, new sp=%p: nxt sp=%p, fp=%p",
	     troll_sp, new_sp, nxt_sp, nxt_fp);
      }
      else if (UI_FLD(intvl,ty) == FrmTy_FP) {
	void** new_fp = getPtrFromPtr(troll_sp, -UI_FLD(intvl,ra_arg));
	computeNext_FPFrame(&nxt_sp, &nxt_fp, UI_ARG(intvl), new_fp);
	TMSG(UNW, "troll align/fp: troll_sp=%p, new fp=%p: nxt sp=%p, fp=%p",
	     troll_sp, new_fp, nxt_sp, nxt_fp);
      }
      else {
	assert(0);
      }
    }
    else if (troll_sp >= nxt_sp) {
      nxt_sp = troll_sp + 1; // ensure progress is made
    }
  }
  // INVARIANT: At this point, 'nxt_intvl' is valid


  // INVARIANT: Ensure we always make progress unwinding the stack...
  bool mayFrameSizeBe0 = frameflg_isset(UI_FLD(intvl,flgs), FrmFlg_RAReg);
  if (!mayFrameSizeBe0 && !isPossibleParentSP(sp, nxt_sp)) {
    TMSG(UNW, "warning: adjust sp b/c nxt_sp=%p < sp=%p", nxt_sp, sp);
    nxt_sp = sp + 1; // should cause trolling on next unw_step
  }


  TMSG(UNW, "next: pc=%p, sp=%p, fp=%p", nxt_pc, nxt_sp, nxt_fp);
  if (MYDBG) { ui_dump(UI_ARG(nxt_intvl)); }

  cursor->pc_unnorm = nxt_pc;
  cursor->pc_norm   = nxt_pc_norm;
  cursor->ra        = nxt_ra;
  cursor->sp        = nxt_sp;
  cursor->bp        = nxt_fp;
  cursor->intvl     = CASTTO_UNW_CURSOR_INTERVAL_t(nxt_intvl);

  return (didTroll) ? STEP_TROLL : STEP_OK;
}
