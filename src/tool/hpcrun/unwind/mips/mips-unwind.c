// -*-Mode: C++;-*- // technically C99
// $Id$

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h> // environ
#include <ucontext.h>
#include <assert.h>

//************************ libmonitor Include Files *************************

// see mips-unwind-cfg.h

//*************************** User Include Files ****************************

#include <include/general.h>

#include "mips-unwind-cfg.h"
#include "mips-unwind-interval.h"

#include "unwind.h"
#include "stack_troll.h"

// FIXME: Abuse the isa library by cherry-picking this special header.
// One possibility is to simply use the ISA lib -- doesn't xed
// internally use C++ stuff?
#include <lib/isa/instructionSets/mips.h>

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
  // For a Linux process, the environment is just above the stack
  // bottom (which may be randomized instead of the traditional 8 MB).
  // With some experimentation, I found that the begin of the
  // 'environ' array is closest to the stack bottom.  This makes sense
  // since the strings are all stored from lower to higher addresses.
  extern char** environ;
  static void** process_stack_bottom = NULL;

  if (!process_stack_bottom) { 
    void** x = (void**)((intptr_t)environ - 1);
    process_stack_bottom = x; // N.B.: atomic update
  }

  return process_stack_bottom; // FIXME: thread stack bottom
#else
  return (void**)monitor_stack_bottom();
#endif
}


//***************************************************************************
// private operations
//***************************************************************************

static inline greg_t
ucontext_getreg(ucontext_t* context, int reg)
{ return context->uc_mcontext.gregs[reg]; }


static inline void*
ucontext_pc(ucontext_t* context)
{ return (void*)context->uc_mcontext.pc; }


static inline void*
ucontext_ra(ucontext_t* context)
{ return (void*)ucontext_getreg(context, REG_RA); }


static inline void**
ucontext_sp(ucontext_t* context)
{ return (void**)ucontext_getreg(context, REG_SP); }


static inline void**
ucontext_fp(ucontext_t* context)
{ return (void**)ucontext_getreg(context, REG_FP); }


//***************************************************************************

static inline void*
getNxtPCFromRA(void* ra)
{
  // both the call instruction and the delay slot have produced effects
  //(uintptr_t)ra - 4 /*delay slot*/ - 4 /*next insn*/;
  return ra;
}


static inline void**
getPtrFromSP(void** sp, int offset)
{ return (void**)((uintptr_t)sp + offset); }


static inline void*
getValFromSP(void** sp, int offset)
{ return *getPtrFromSP(sp, offset); }


static inline void*
getValFromFP(void** fp, int offset)
{ return *getPtrFromSP(fp, offset); }


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
  *nxt_sp = getPtrFromSP(sp, intvl->sp_arg);

  if (intvl->fp_arg != unwarg_NULL) {
    *nxt_fp = getValFromSP(sp, intvl->fp_arg);
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
    *nxt_sp = getValFromFP(fp, intvl->sp_arg);
  }
  else {
    *nxt_sp = fp;
  }
  
  if (intvl->fp_arg != unwarg_NULL) {
    *nxt_fp = getValFromFP(fp, intvl->fp_arg);
  }
}


//***************************************************************************
// interface functions
//***************************************************************************

void*
context_pc(void* context)
{ 
  ucontext_t* ctxt = context;
  return ucontext_pc(ctxt);
}


void
unw_init(void)
{
  HPC_IFNO_UNW_LITE(csprof_interval_tree_init();)
}


int 
unw_get_reg(unw_cursor_t* cursor, int reg_id, void **reg_value)
{
  assert(reg_id == UNW_REG_IP);
  *reg_value = cursor->pc;
  return 0;
}


void 
unw_init_cursor(void* context, unw_cursor_t* cursor)
{
  ucontext_t* ctxt = (ucontext_t*)context;

  cursor->pc = ucontext_pc(ctxt);
  cursor->ra = ucontext_ra(ctxt);
  cursor->sp = ucontext_sp(ctxt);
  cursor->bp = ucontext_fp(ctxt);

  UNW_INTERVAL_t intvl = demand_interval(cursor->pc);
  cursor->intvl = CASTTO_UNW_CURSOR_INTERVAL_t(intvl);

  if (!UI_IS_NULL(intvl)) {
    if (frameflg_isset(UI_FLD(intvl,flgs), FrmFlg_RAReg)) {
      cursor->ra = (void*)ucontext_getreg(context, UI_FLD(intvl,ra_arg));
    }
    if (frameflg_isset(UI_FLD(intvl,flgs), FrmFlg_FPInV0)) {
      // FIXME: it would be nice to preserve the parent FP
    }
  }

  TMSG(UNW, "init: pc=%p, ra=%p, sp=%p, fp=%p", 
       cursor->pc, cursor->ra, cursor->sp, cursor->bp);
  if (MYDBG) { ui_dump(UI_ARG(intvl), 1); }
}


int 
unw_step(unw_cursor_t* cursor)
{
  // current frame:
  void*  pc = cursor->pc;
  void** sp = cursor->sp;
  void** fp = cursor->bp;
  UNW_INTERVAL_t intvl = CASTTO_UNW_INTERVAL_t(cursor->intvl);

  // next (parent) frame
  void*  nxt_pc = pc_NULL;
  void** nxt_sp = NULL;
  void** nxt_fp = NULL;
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
    nxt_pc = cursor->ra;
  }
  else if (UI_FLD(intvl,ty) == FrmTy_SP) {
    nxt_pc = getValFromSP(sp, UI_FLD(intvl,ra_arg));
  }
  else if (UI_FLD(intvl,ty) == FrmTy_FP) {
    if (isPossibleFP(sp, fp)) { // FP is our weak spot
      nxt_pc = getValFromFP(fp, UI_FLD(intvl,ra_arg));
    }
  }
  else {
    assert(0);
  }
  // INVARIANT: nxt_pc has been overwritten or is pc_NULL

  if (nxt_pc == NULL) {
    // Allegedly, MIPS API promises NULL RA for outermost frame
    // NOTE: we are already past the last valid PC
    TMSG(UNW, "stop: pc=%p", nxt_pc);
    return STEP_STOP;
  }

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

  bool didTroll = false;
  if (nxt_pc != pc_NULL) {
    nxt_intvl = demand_interval(getNxtPCFromRA(nxt_pc));
  }

  if (UI_IS_NULL(nxt_intvl)) {
    // nxt_pc is invalid for some reason... try trolling
    TMSG(UNW, "troll: bad pc=%p; cur sp=%p, fp=%p...", nxt_pc, sp, fp);

    uint troll_pc_ofst;
    uint ret = stack_troll(sp, &troll_pc_ofst);
    if (!ret) {
      TMSG(UNW, "error: troll failed");
      return STEP_ERROR;
    }
    void** troll_sp = (void**)getPtrFromSP(sp, troll_pc_ofst);
    
    nxt_intvl = demand_interval(getNxtPCFromRA(*troll_sp));
    if (UI_IS_NULL(nxt_intvl)) {
      TMSG(UNW, "error: troll_pc=%p failed", *troll_sp);
      return STEP_ERROR;
    }
    didTroll = true;

    nxt_pc = *troll_sp;

    if (!frameflg_isset(UI_FLD(intvl,flgs), FrmFlg_RAReg) 
	&& UI_FLD(intvl,ra_arg) != unwarg_NULL) {
      // realign sp/fp and recompute nxt_sp, nxt_fp
      if (UI_FLD(intvl,ty) == FrmTy_SP) {
	void** new_sp = getPtrFromSP(troll_sp, -UI_FLD(intvl,ra_arg));
	computeNext_SPFrame(&nxt_sp, &nxt_fp, UI_ARG(intvl), new_sp, NULL);
	TMSG(UNW, "troll align/sp: troll_sp=%p, new sp=%p: nxt sp=%p, fp=%p",
	     troll_sp, new_sp, nxt_sp, nxt_fp);
      }
      else if (UI_FLD(intvl,ty) == FrmTy_FP) {
	void** new_fp = getPtrFromSP(troll_sp, -UI_FLD(intvl,ra_arg));
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
  bool frameSizeMayBe0 = frameflg_isset(UI_FLD(intvl,flgs), FrmFlg_RAReg);
  if (!frameSizeMayBe0 && !isPossibleParentSP(sp, nxt_sp)) {
    TMSG(UNW, "warning: adjust sp b/c nxt_sp=%p < sp=%p", nxt_sp, sp);
    nxt_sp = sp + 1; // should cause trolling on next unw_step
  }


  TMSG(UNW, "next: pc=%p, sp=%p, fp=%p", nxt_pc, nxt_sp, nxt_fp);
  if (MYDBG) { ui_dump(UI_ARG(nxt_intvl), 1); }

  cursor->pc = nxt_pc;
  cursor->sp = nxt_sp;
  cursor->bp = nxt_fp;
  cursor->ra = NULL; // always NULL after unw_step() completes
  cursor->intvl = CASTTO_UNW_CURSOR_INTERVAL_t(nxt_intvl);

  return (didTroll) ? STEP_TROLL : STEP_OK;
}

