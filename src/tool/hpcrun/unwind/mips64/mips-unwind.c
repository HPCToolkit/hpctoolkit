// -*-Mode: C++;-*- // technically C99
// $Id$

//************************* System Include Files ****************************

#include <ucontext.h>
#include <assert.h>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "unwind.h"

// FIXME: Abuse the isa library by cherry-picking this special header.
// One possibility is to simply use the ISA lib -- doesn't xed
// internally use C++ stuff?
#include <lib/isa/instructionSets/mips.h>

//*************************** Forward Declarations **************************

#define DBG 0

static inline greg_t
ucontext_getreg(ucontext_t* context, int reg)
{ return context->uc_mcontext.gregs[reg]; }


static inline void*
ucontext_pc(ucontext_t* context)
{ return context->uc_mcontext.pc; }


static inline void*
ucontext_ra(ucontext_t* context, int reg)
{ return (void*)ucontext_getreg(context, MIPS_RA); }


static inline void**
ucontext_sp(ucontext_t* context, int reg)
{ return (void*)ucontext_getreg(context, MIPS_SP); }


static inline void**
ucontext_fp(ucontext_t* context, int reg)
{ return (void*)ucontext_getreg(context, MIPS_FP); }

static inline void*
getRAFromSP(void** sp, int offset)
{ return (*(sp + offset)); }

static inline void*
getSPFromSP(void** sp, int offset)
{ return (sp + offset); }


//***************************************************************************
// interface functions
//***************************************************************************

void
unw_init_arch(void)
{
}


int 
unw_get_reg_arch(unw_cursor_t* cursor, int reg_id, void **reg_value)
{
  assert(reg_id == UNW_REG_IP);
  *reg_value = cursor->pc;
  return 0;
}


void 
unw_init_cursor(void* context, unw_cursor_t* cursor)
{
  ucontext_t* ctxt = (ucontext_t*)context;

  cursor->pc = context_pc(ctxt);
  cursor->ra = context_ra(ctxt);
  cursor->sp = context_sp(ctxt);
  cursor->fp = context_fp(ctxt);

  cursor->intvl = csprof_addr_to_interval(cursor->pc);

  // FIXME: if SP is register-relative... obtain
  // FIXME: if FP is active, 

  TMSG(UNW_INIT,"frame pc = %p, frame sp = %p, frame ra = %p", 
       cursor->pc, cursor->sp, cursor->ra);
  if (DBG) { dump_ui(cursor->intvl, 1); }
  TMSG(UNW_INIT,"returned interval = %p", cursor->intvl);
}


int 
unw_step(unw_cursor_t *cursor)
{
  // current frame
  void*  pc = cursor->pc;
  void** ra = cursor->ra;
  void** sp = cursor->sp;
  void** fp = cursor->fp;

  // next frame
  void*  next_pc = 0;
  void** next_sp = 0;
  void** next_fp = 0;

  //-----------------------------------------------------------
  // check for outermost frame
  //-----------------------------------------------------------
  if (monitor_in_start_func_wide(pc)) {
    TMSG(UNWIND,"monitor in start func wide, pc=%p", pc);
    return 0;
  }
  
  //-----------------------------------------------------------
  // compute the return address for the caller's frame
  //-----------------------------------------------------------
  if (cursor->intvl->ra_status == RA_REGISTER) {
    next_pc = cursor->ra;
  }
  else if (cursor->intvl->ra_status == RA_SP_RELATIVE) {
    next_pc = getRAFromSP(cursor->sp, intvl->sp_ra_pos);
  }
  else if (cursor->intvl->ra_status == RA_FP_RELATIVE) {
    assert(0); // FIXME
  }

  //-----------------------------------------------------------
  // compute the stack pointer for the caller's frame.
  //-----------------------------------------------------------
  // fixed size
  next_sp = getSPFromSP(cursor->sp, intvl->sp_sp_pos);
  // in a register
  // on the stack


  TMSG(UNWIND,"candidate next_pc = %p, candidate next_sp = %p", next_pc, next_sp);
  if ( !(next_sp > sp) ) {
    TMSG(UNWIND,"next sp = %p < current sp = %p", next_sp, sp);
    return 0;
  }

  //-----------------------------------------------------------
  // compute unwind information for the caller's pc
  //-----------------------------------------------------------
  cursor->intvl = csprof_addr_to_interval(next_pc);

  //-----------------------------------------------------------
  // the pc is invalid if no unwind information is  available
  //-----------------------------------------------------------
  if (!cursor->intvl) {
    TMSG(UNWIND,"next_pc = %p has no valid interval, continuing to look ...", next_pc);
    if (next_sp >= monitor_stack_bottom()) {
      TMSG(UNWIND,"next sp (%p) >= monitor_stack_bottom (%p)", next_sp, monitor_stack_bottom());
      return 0;
    }

    //-------------------------------------------------------------------
    // there isn't a valid PC saved in the return address slot in my 
    // caller's frame.  try one frame deeper. 
    //
    // NOTE: this should only happen in the top frame on the stack.
    //-------------------------------------------------------------------
    next_pc = PC_FROM_FRAME_FP(*next_fp);
    TMSG(UNWIND,"skipping up one frame f candidate fp = %p ==> next pc = %p", next_fp, next_pc);
    cursor->intvl = csprof_addr_to_interval(next_pc);
  }

  if (!cursor->intvl) {
    TMSG(UNWIND,"candidate next_pc = %p did not have an interval",next_pc);
    return 0;
  }

  cursor->pc = next_pc;
  cursor->sp = next_sp;
  cursor->ra = NULL; // always NULL after unw_step() completes

  TMSG(UNWIND,"step goes forward w pc = %p, fp = %p",next_pc,next_fp);
  return 1;
}

