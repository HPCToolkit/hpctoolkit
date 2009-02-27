// -*-Mode: C++;-*- // technically C99
// $Id$

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

#include "pmsg.h"
#include "splay.h"
#include "ui_tree.h"

// FIXME: see note in mips-unwind.c
#include <lib/isa/instructionSets/ppc.h>


//***************************************************************************
// forward declarations
//***************************************************************************

#define MYDBG 0


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
getNxtPCFromFP(void** fp)
{
  static const int RA_OFFSET_FROM_FP = 1;
  return *(fp + RA_OFFSET_FROM_FP);
}


static inline bool
isPossibleNonBottomFP(void** fp)
{
  // Stacks grow down, so frame pointer is at a higher address
  // N.B.: fp is strictly less than stack bottom
  return (monitor_stack_bottom() > (void*)fp /*&& fp >= sp*/);
}


static inline bool
isPossibleParentFP(void** fp, void** parent_fp)
{
  // Stacks grow down, so outer frames are at higher addresses
  return (parent_fp > fp); // assume frame size is not 0
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
  csprof_interval_tree_init();
}


int 
unw_get_reg(unw_cursor_t *cursor, int reg_id, void **reg_value)
{
  assert(reg_id == UNW_REG_IP);
  *reg_value = cursor->pc;
  return 0;
}


void 
unw_init_cursor(unw_cursor_t *cursor, void *context)
{
  ucontext_t* ctxt = (ucontext_t*)context;

  cursor->pc = ucontext_pc(ctxt);
  cursor->ra = NULL;
  cursor->sp = NULL;
  cursor->bp = ucontext_fp(ctxt);

  unw_interval_t* intvl = (unw_interval_t*)csprof_addr_to_interval(cursor->pc);
  cursor->intvl = (splay_interval_t*)intvl;
    
  if (intvl->ra_status == RA_REGISTER) {
    if (intvl->ra_arg == PPC_REG_R0) {
      cursor->ra = (void*)(ctxt->uc_mcontext.regs->gpr[PPC_REG_R0]);
    }
    else if (intvl->ra_arg == PPC_REG_LR) {
      cursor->ra = (void*)(ctxt->uc_mcontext.regs->link);
    }
    else {
      assert(0);
    }
  }
  
  TMSG(UNW, "init: pc=%p, ra=%p, sp=%p, fp=%p", 
       cursor->pc, cursor->ra, cursor->sp, cursor->bp);
  if (MYDBG) { ui_dump(intvl, 1); }
}


int 
unw_step(unw_cursor_t *cursor)
{
  // current frame
  void*  pc = cursor->pc;
  void** sp = cursor->sp; // unused
  void** fp = cursor->bp;
  unw_interval_t* intvl = (unw_interval_t*)(cursor->intvl);
  
  // next (parent) frame
  void*  nxt_pc = NULL;
  void** nxt_sp = NULL; // unused
  void** nxt_fp = NULL;
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
  
  if ((void*)fp >= monitor_stack_bottom()) {
    TMSG(UNW, "stop: fp (%p) >= unw_stack_bottom", fp);
    return STEP_STOP;
  }


  //-----------------------------------------------------------
  // compute RA (return address) for the caller's frame
  //-----------------------------------------------------------
  if (intvl->ra_status == RA_REGISTER) {
    // RA should be in a register only on the first call to unw_step()
    // (or the first call after going through a signal trampoline)
    nxt_pc = cursor->ra;
  }
  else {
    // INVARIANT: RA is saved in my caller's stack frame
    if (intvl->bp_status != BP_SAVED) {
      // FP already points to caller's stack frame
      nxt_pc = getNxtPCFromFP(fp);
    }
    else {
      // FP points to parent's FP
      nxt_pc = getNxtPCFromFP((void**) *fp);
    }
  }

  //-----------------------------------------------------------
  // compute FP (frame pointer) for the caller's frame
  //-----------------------------------------------------------
  if (intvl->bp_status != BP_SAVED) {
    // FP already points to caller's stack frame
    nxt_fp = fp;
  }
  else {
    // FP points to parent's FP
    nxt_fp = *fp;
  }

  //-----------------------------------------------------------
  // compute unwind information for the caller's pc
  //-----------------------------------------------------------
  nxt_intvl = (unw_interval_t*)csprof_addr_to_interval(nxt_pc);

  // if nxt_pc is invalid for some reason...
  if (!nxt_intvl) {
    TMSG(UNW, "warning: bad nxt pc=%p; sp=%p, fp=%p...", nxt_pc, sp, fp);

    //-------------------------------------------------------------------
    // assume there is no valid PC saved in the return address slot in my 
    // caller's frame.  try one frame deeper. 
    //
    // NOTE: this should only happen in the top frame on the stack.
    //-------------------------------------------------------------------
    void** new_fp = NULL;
    if (isPossibleNonBottomFP(nxt_fp)) {
      new_fp = *nxt_fp;
      nxt_pc = getNxtPCFromFP(new_fp);
    
      nxt_intvl = (unw_interval_t*)csprof_addr_to_interval(nxt_pc);
    }
     
    if (!nxt_intvl) {
      TMSG(UNW, "error: skip-frame failed: nxt pc=%p, fp=%p; new fp=%p", 
	   nxt_pc, nxt_fp, new_fp);
      return STEP_ERROR;
    }

    nxt_fp = new_fp;
    TMSG(UNW, "skip-frame: nxt pc=%p, fp=%p", nxt_pc, nxt_fp);
  }
  // INVARIANT: At this point, 'nxt_intvl' is valid


  // INVARIANT: Ensure we always make progress unwinding the stack...
  bool frameSizeMayBe0 = (intvl->bp_status != BP_SAVED);
  if (!frameSizeMayBe0 && !isPossibleParentFP(fp, nxt_fp)) {
    // FIXME: possibly combine this with trolling
    //   TMSG(UNW, "warning: adjust sp b/c nxt_sp=%p < sp=%p", nxt_sp, sp);
    //   nxt_fp = fp + 1
    TMSG(UNW, "error: loop! nxt_fp=%p, fp=%p", nxt_fp, fp);
    return STEP_ERROR;
  }


  TMSG(UNW, "next: pc=%p, sp=%p, fp=%p", nxt_pc, nxt_sp, nxt_fp);
  if (MYDBG) { ui_dump(nxt_intvl, 1); }

  cursor->pc = nxt_pc;
  cursor->ra = nxt_ra;
  cursor->sp = nxt_sp;
  cursor->bp = nxt_fp;
  cursor->intvl = (splay_interval_t*)nxt_intvl;

  return STEP_OK;
}

