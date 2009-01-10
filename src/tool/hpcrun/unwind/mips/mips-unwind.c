// -*-Mode: C++;-*- // technically C99
// $Id$

//************************* System Include Files ****************************

#include <ucontext.h>
#include <assert.h>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "general.h"
#include "mem.h"
#include "pmsg.h"
#include "monitor.h"

#include "unwind.h"
#include "splay.h"
#include "splay-interval.h"
#include "stack_troll.h"

#include "mips-unwind-interval.h"


// FIXME: Abuse the isa library by cherry-picking this special header.
// One possibility is to simply use the ISA lib -- doesn't xed
// internally use C++ stuff?
#include <lib/isa/instructionSets/mips.h>

//*************************** Forward Declarations **************************

#define MYDBG 0

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
ucontext_bp(ucontext_t* context)
{ return (void**)ucontext_getreg(context, REG_FP); }


//***************************************************************************

static inline void**
getSPFromSP(void** sp, int offset)
{ return (void**)((uintptr_t)sp + offset); }


static inline void*
getRAFromSP(void** sp, int offset)
{ return *getSPFromSP(sp, offset); }


static inline void*
getRAFromBP(void** bp, int offset)
{ return *(void**)((uintptr_t)bp - offset); }


static inline bool
isPossibleParentSP(void** sp, void** parent_sp)
{
  // Stacks grow down, so outer frames are at higher addresses
  return (parent_sp > sp);
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
  csprof_interval_tree_init();
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
  cursor->bp = ucontext_bp(ctxt);

  cursor->intvl = csprof_addr_to_interval(cursor->pc);
  unw_interval_t* intvl = (unw_interval_t*)cursor->intvl;

  if (intvl->flgs == FrmFlg_RAReg) {
    cursor->ra = (void*)ucontext_getreg(context, intvl->ra_arg);
  }

  TMSG(UNW_INIT,"pc=%p, ra=%p, sp=%p, bp=%p (intvl: %p)", 
       cursor->pc, cursor->ra, cursor->sp, cursor->bp, intvl);
  if (MYDBG) { dump_ui(intvl, 1); }
}


int 
unw_step(unw_cursor_t* cursor)
{
  // current frame:
  void*  pc = cursor->pc;
  void** sp = cursor->sp;
  void** bp = cursor->bp;
  unw_interval_t* intvl = (unw_interval_t*)cursor->intvl;

  // next (parent) frame
  void*  nxt_pc = 0;
  void** nxt_sp = 0;
  void** nxt_bp = 0;
  unw_interval_t* nxt_intvl = NULL;
  
  //-----------------------------------------------------------
  // check for outermost frame (return STEP_STOP only after outermost
  // frame has been identified as valid)
  //-----------------------------------------------------------
  if (monitor_in_start_func_wide(pc)) {
    TMSG(UNW, "stop: monitor_in_start_func_wide, pc=%p", pc);
    return STEP_STOP;
  }

  if ( (void*)sp >= monitor_stack_bottom() ) {
    TMSG(UNW,"stop: sp (%p) >= monitor_stack_bottom", sp);
    return STEP_STOP;
  }
  
  //-----------------------------------------------------------
  // compute RA (return address) for the caller's frame
  //-----------------------------------------------------------
  if (intvl->flgs == FrmFlg_RAReg) {
    // RA should be in a register only on the first call to unw_step()
    nxt_pc = cursor->ra;
  }
  else if (intvl->ty == FrmTy_SP) {
    nxt_pc = getRAFromSP(sp, intvl->ra_arg);
  }
  else if (intvl->ty == FrmTy_BP) {
    nxt_pc = getRAFromBP(bp, intvl->ra_arg);
    assert(0); // FIXME
  }
  else {
    assert(0);
  }

  //-----------------------------------------------------------
  // compute SP (stack pointer) for the caller's frame.
  //-----------------------------------------------------------
  if (intvl->ty == FrmTy_SP) {
    nxt_sp = getSPFromSP(sp, intvl->sp_pos);
  }
  else if (intvl->ty == FrmTy_BP) {
    //nxt_sp = getSPFromBP(bp, intvl->ra_arg);
    assert(0); // FIXME
  }
  else {
    assert(0);
  }

  //-----------------------------------------------------------
  // compute BP (frame pointer) for the caller's frame.
  //-----------------------------------------------------------
  // FIXME



  //-----------------------------------------------------------
  // compute unwind information for the caller's pc
  //-----------------------------------------------------------

  if (!nxt_pc) {
    // Allegedly, MIPS API promises NULL RA for outermost frame
    // NOTE: we are already past the last valid PC
    TMSG(UNW,"stop: pc=%p", nxt_pc);
    return STEP_STOP;
  }
  
  bool didTroll = false;
  nxt_intvl = (unw_interval_t*)csprof_addr_to_interval(nxt_pc);

  if (!nxt_intvl) {
    // nxt_pc is invalid for some reason... try trolling
    
    uint troll_pc_pos;
    uint ret = stack_troll((char**)sp, &troll_pc_pos);
    if (!ret) {
      TMSG(UNW,"error: troll failed");
      return STEP_ERROR;
    }
    void** troll_sp = (void**)getSPFromSP(sp, troll_pc_pos);
    
    nxt_intvl = (unw_interval_t*)csprof_addr_to_interval(*troll_sp);
    if (!nxt_intvl) {
      TMSG(UNW,"error: nxt_pc=%p troll_pc=%p sp=%p", nxt_pc, *troll_sp, sp);
      return STEP_ERROR;
    }
    didTroll = true;

    nxt_pc = *troll_sp;
    if (troll_sp > nxt_sp) {
      nxt_sp = troll_sp + 1;
    }
  }
  // INVARIANT: At this point, 'nxt_intvl' is valid

  if (!isPossibleParentSP(sp, nxt_sp)) {
    // FIXME try trolling and update sp...
    TMSG(UNW,"error: nxt_sp=%p < sp=%p", nxt_sp, sp);
    return STEP_ERROR;
  }

  TMSG(UNW_INIT,"next: pc=%p, sp=%p, bp=%p (intvl: %p)", 
       nxt_pc, nxt_sp, nxt_bp, nxt_intvl);

  cursor->pc = nxt_pc;
  cursor->sp = nxt_sp;
  cursor->bp = nxt_bp;
  cursor->ra = NULL; // always NULL after unw_step() completes
  cursor->intvl = (splay_interval_t*)nxt_intvl;

  return (didTroll) ? STEP_TROLL : STEP_OK;
}

