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

// note: 
//     when cross compiling, this version MUST BE compiled for
//     the target system, as the include file that defines the
//     structure for the contexts may be different for the
//     target system vs the build system.

//***************************************************************************
// system include files 
//***************************************************************************

#include <stdio.h>
#include <setjmp.h>
#include <stdbool.h>
#include <assert.h>

#include <sys/types.h>
#include <unistd.h> // for getpid


//***************************************************************************
// libmonitor includes
//***************************************************************************

#include <monitor.h>


//***************************************************************************
// local include files 
//***************************************************************************

#include <include/gcc-attr.h>
#include <x86-decoder.h>

#include <hpcrun/epoch.h>
#include "stack_troll.h"
#include "thread_use.h"

#include <unwind/common/unwind.h>
#include <unwind/common/backtrace.h>
#include "splay.h"
#include "ui_tree.h"

#include <hpcrun/thread_data.h>
#include "x86-unwind-interval.h"
#include "x86-validate-retn-addr.h"

#include <messages/messages.h>
#include <messages/debug-flag.h>
#include "main.h"



//****************************************************************************
// global data 
//****************************************************************************

int debug_unw = 0;



//****************************************************************************
// local data 
//****************************************************************************

// flag value won't matter environment var set
static int DEBUG_WAIT_BEFORE_TROLLING = 1; 

static int DEBUG_NO_LONGJMP = 0;



//****************************************************************************
// forward declarations 
//****************************************************************************

#define MYDBG 0


static void 
update_cursor_with_troll(unw_cursor_t *cursor, int offset);

static int 
hpcrun_check_fence(void *ip);

static void 
drop_sample(void);

static void 
_drop_sample(bool no_backtrace);

static int
unw_step_prefer_sp(void);

static step_state 
unw_step_sp(unw_cursor_t *cursor);

static step_state
unw_step_bp(unw_cursor_t *cursor);

static step_state
unw_step_std(unw_cursor_t *cursor);

static step_state
t1_dbg_unw_step(unw_cursor_t *cursor);

static step_state (*dbg_unw_step)(unw_cursor_t *cursor) = t1_dbg_unw_step;


//***************************************************************************
// macros
//***************************************************************************

#if defined(__LIBCATAMOUNT__)
#undef __CRAYXT_CATAMOUNT_TARGET
#define __CRAYXT_CATAMOUNT_TARGET
#endif

#define GET_MCONTEXT(context) (&((ucontext_t *)context)->uc_mcontext)

//-------------------------------------------------------------------------
// define macros for extracting pc, bp, and sp from machine contexts. these
// macros bridge differences between machine context representations for
// Linux and Catamount
//-------------------------------------------------------------------------
#ifdef __CRAYXT_CATAMOUNT_TARGET

#define MCONTEXT_PC(mctxt) ((void *)   mctxt->sc_rip)
#define MCONTEXT_BP(mctxt) ((void **)  mctxt->sc_rbp)
#define MCONTEXT_SP(mctxt) ((void **)  mctxt->sc_rsp)

#else

#define MCONTEXT_REG(mctxt, reg) (mctxt->gregs[reg])
#define MCONTEXT_PC(mctxt) ((void *)  MCONTEXT_REG(mctxt, REG_RIP))
#define MCONTEXT_BP(mctxt) ((void **) MCONTEXT_REG(mctxt, REG_RBP))
#define MCONTEXT_SP(mctxt) ((void **) MCONTEXT_REG(mctxt, REG_RSP))

#endif


//****************************************************************************
// interface functions
//****************************************************************************

void*
hpcrun_context_pc(void* context)
{
  mcontext_t *mc = GET_MCONTEXT(context);
  return MCONTEXT_PC(mc);
}


void
hpcrun_unw_init(void)
{
  x86_family_decoder_init();
  hpcrun_interval_tree_init();
}


int 
hpcrun_unw_get_reg(unw_cursor_t *cursor, unw_reg_code_t reg_id, void **reg_value)
{
  //
  // only implement 1 reg for the moment.
  //  add more if necessary
  //
  switch (reg_id) {
    case UNW_REG_IP:
      *reg_value = cursor->pc;
      break;
    default:
      return ~0;
  }
  return 0;
}


void 
hpcrun_unw_init_cursor(unw_cursor_t* cursor, void* context)
{
  mcontext_t *mc = GET_MCONTEXT(context);

  cursor->pc 	 = MCONTEXT_PC(mc); 
  cursor->bp 	 = MCONTEXT_BP(mc);
  cursor->sp 	 = MCONTEXT_SP(mc);
  cursor->ra_loc = NULL;

  TMSG(UNW, "init: pc=%p, ra_loc=%p, sp=%p, bp=%p", 
       cursor->pc, cursor->ra_loc, cursor->sp, cursor->bp);

  cursor->flags = 0; // trolling_used
  cursor->intvl = hpcrun_addr_to_interval(cursor->pc);

  if (!cursor->intvl) {
    // TMSG(TROLL,"UNW INIT calls stack troll");
    update_cursor_with_troll(cursor, 0);
  }

  if (MYDBG) { dump_ui((unwind_interval *)cursor->intvl, 0); }
}

//
// Unwinder support for trampolines augments the
// cursor with 'ra_loc' field.
//
// This is NOT part of libunwind interface, so
// it does not use the unw_get_reg.
//
// Instead, we use the following procedure to abstractly
// fetch the ra_loc
//

void*
hpcrun_unw_get_ra_loc(unw_cursor_t* cursor)
{
  return cursor->ra_loc;
}


static step_state
hpcrun_unw_step_real(unw_cursor_t *cursor)
{

  //-----------------------------------------------------------
  // check if we have reached the end of our unwind, which is
  // demarcated with a fence. 
  //-----------------------------------------------------------
  if (hpcrun_check_fence(cursor->pc)){
    TMSG(UNW,"current pc in monitor fence", cursor->pc);
    return STEP_STOP;
  }

  // current frame  
  void** bp = cursor->bp;
  void*  sp = cursor->sp;
  void*  pc = cursor->pc;
  unwind_interval* uw = (unwind_interval *)cursor->intvl;

  int unw_res;

  if (!uw) return STEP_ERROR;

  switch (uw->ra_status){
  case RA_SP_RELATIVE:
    unw_res = unw_step_sp(cursor);
    break;
    
  case RA_BP_FRAME:
    unw_res = unw_step_bp(cursor);
    break;
    
  case RA_STD_FRAME:
    unw_res = unw_step_std(cursor);
    break;

  default:
    EMSG("error: ILLEGAL UNWIND INTERVAL");
    dump_ui((unwind_interval *)cursor->intvl, 0);
    assert(0);
  }
  if (unw_res == STEP_STOP_WEAK) unw_res = STEP_STOP; 

  if (unw_res != -1) {
    //TMSG(UNW,"=========== unw_step Succeeds ============== ");
    return unw_res;
  }
  
  PMSG_LIMIT(TMSG(TROLL,"error: unw_step: pc=%p, bp=%p, sp=%p", pc, bp, sp));
  dump_ui_troll(uw);

  if (ENABLED(TROLL_WAIT)) {
    fprintf(stderr,"Hit troll point: attach w gdb to %d\n"
            "Maybe call dbg_set_flag(DBG_TROLL_WAIT,0) after attached\n",
	    getpid());
    
    // spin wait for developer to attach a debugger and clear the flag 
    while(DEBUG_WAIT_BEFORE_TROLLING);  
  }
  
  update_cursor_with_troll(cursor, 1);
  return STEP_TROLL;
}

#undef _MM
#define _MM(a,v) [v] = 0

size_t hpcrun_validation_counts[] = {
#include "validate_return_addr.src"
};


void
hpcrun_validation_summary(void)
{
  AMSG("VALIDATION: Confirmed: %ld, Probable: %ld (indirect: %ld, tail: %ld, etc: %ld), Wrong: %ld",
       hpcrun_validation_counts[UNW_ADDR_CONFIRMED],
       hpcrun_validation_counts[UNW_ADDR_PROBABLE_INDIRECT] +
         hpcrun_validation_counts[UNW_ADDR_PROBABLE_TAIL] +
         hpcrun_validation_counts[UNW_ADDR_PROBABLE],
       hpcrun_validation_counts[UNW_ADDR_PROBABLE_INDIRECT],
       hpcrun_validation_counts[UNW_ADDR_PROBABLE_TAIL],
       hpcrun_validation_counts[UNW_ADDR_PROBABLE],
       hpcrun_validation_counts[UNW_ADDR_WRONG]);
}


static void
vrecord(void *from, void *to, validation_status vstat)
{
  hpcrun_validation_counts[vstat]++;

  if ( ENABLED(VALID_RECORD_ALL) || (vstat == UNW_ADDR_WRONG) ){
    TMSG(UNW_VALID,"%p->%p (%s)", from, to, vstat2s(vstat));
  }
}


step_state
hpcrun_unw_step(unw_cursor_t *cursor)
{
  if ( ENABLED(DBG_UNW_STEP) ){
    return dbg_unw_step(cursor);
  }
  
  unw_cursor_t saved = *cursor;
  step_state rv = hpcrun_unw_step_real(cursor);
  if ( ENABLED(UNW_VALID) ) {
    if (rv == STEP_OK) {
      // try to validate all calls, except the one at the base of the call stack from libmonitor.
      // rather than recording that as a valid call, it is preferable to ignore it.
      if (!monitor_in_start_func_wide(cursor->pc)) {
	validation_status vstat = deep_validate_return_addr(cursor->pc, (void *) &saved);
	vrecord(saved.pc,cursor->pc,vstat);
      }
    }
  }
  return rv;
}


void
hpcrun_unw_throw(void)
{
  _drop_sample(true);
}


//****************************************************************************
// hpcrun_unw_step helpers
//****************************************************************************

// FIXME: make this a selectable paramter, so that all manner of strategies 
// can be selected
static int
unw_step_prefer_sp(void)
{
  if (ENABLED(PREFER_SP)){
    return 1;
  } else {
    return 0;
  }
  // return cursor->flags; // trolling_used
}


static step_state
unw_step_sp(unw_cursor_t *cursor)
{
  TMSG(UNW_STRATEGY,"Using SP step");

  // void *stack_bottom = monitor_stack_bottom();

  // current frame
  void** bp = cursor->bp;
  void*  sp = cursor->sp;
  void*  pc = cursor->pc;
  unwind_interval* uw = (unwind_interval *)cursor->intvl;
  
  TMSG(UNW,"step_sp: bp=%p, sp=%p, pc=%p", bp, sp, pc);
  if (MYDBG) { dump_ui(uw, 0); }

  void** next_bp = NULL;
  void** next_sp = (void **)(sp + uw->sp_ra_pos);
  void*  ra_loc  = (void*) next_sp;
  void*  next_pc  = *next_sp;

  TMSG(UNW,"sp potential advance cursor: next_sp=%p ==> next_pc = %p",
       next_sp, next_pc);

  if (uw->bp_status == BP_UNCHANGED){
    next_bp = bp;
    TMSG(UNW,"sp unwind step has BP_UNCHANGED ==> next_bp=%p", next_bp);
  } else {
    //-----------------------------------------------------------
    // reload the candidate value for the caller's BP from the 
    // save area in the activation frame according to the unwind 
    // information produced by binary analysis
    //-----------------------------------------------------------
    next_bp = (void **)(sp + uw->sp_bp_pos);
    TMSG(UNW,"sp unwind next_bp loc = %p", next_bp);
    next_bp  = *next_bp; 
    TMSG(UNW,"sp unwind next_bp val = %p", next_bp);

    //-----------------------------------------------------------
    // if value of BP reloaded from the save area does not point 
    // into the stack, then it cannot possibly be useful as a frame 
    // pointer in the caller or any of its ancesters.
    //
    // if the value in the BP register points into the stack, then 
    // it might be useful as a frame pointer. in this case, we have 
    // nothing to lose by assuming that our binary analysis for 
    // unwinding might have been mistaken and that the value in 
    // the register is the one we might want. 
    //
    // 19 December 2007 - John Mellor-Crummey
    //-----------------------------------------------------------
    if (((unsigned long) next_bp < (unsigned long) sp) && 
        ((unsigned long) bp > (unsigned long) sp)){
      next_bp = bp;
      TMSG(UNW,"sp unwind bp sanity check fails."
	   " Resetting next_bp to current bp = %p", next_bp);
    }
  }
  next_sp += 1;
  cursor->intvl = hpcrun_addr_to_interval(((char *)next_pc) - 1);

  if (! cursor->intvl){
    if (((void *)next_sp) >= monitor_stack_bottom()){
      TMSG(UNW,"No next interval, and next_sp >= stack bottom,"
	   " so stop unwind ...");
      return STEP_STOP_WEAK;
    } else {
      TMSG(UNW,"No next interval, step fails");
      return STEP_ERROR;
    }
  } else {
    // sanity check to avoid infinite unwind loop
    if (next_sp <= cursor->sp){
      TMSG(INTV_ERR,"@ pc = %p. sp unwind does not advance stack." 
	   " New sp = %p, old sp = %p", cursor->pc, next_sp, cursor->sp);
      return STEP_ERROR;
    }
    cursor->pc 	   = next_pc;
    cursor->bp 	   = next_bp;
    cursor->sp 	   = next_sp;
    cursor->ra_loc = ra_loc;
  }

  TMSG(UNW,"==== sp advance ok ===");
  return STEP_OK;
}


static step_state
unw_step_bp(unw_cursor_t *cursor)
{
  void *sp, **bp, *pc; 
  void **next_sp, **next_bp, *next_pc;

  unwind_interval *uw;

  TMSG(UNW_STRATEGY,"Using BP step");
  // current frame
  bp = cursor->bp;
  sp = cursor->sp;
  pc = cursor->pc;
  uw = (unwind_interval *)cursor->intvl;

  TMSG(UNW,"cursor in ==> bp=%p, sp=%p, pc=%p", bp, sp, pc);
  TMSG(UNW,"unwind interval in below:");
  if (MYDBG) { dump_ui(uw, 0); }

  if (!(sp <= (void*) bp)) {
    TMSG(UNW_STRATEGY_ERROR,"bp unwind attempted, but incoming bp(%p) was not"
	 " >= sp(%p)", bp, sp);
    return STEP_ERROR;
  }
  if (DISABLED(OMP_SKIP_MSB)) {
    if (!((void *)bp < monitor_stack_bottom())) {
      TMSG(UNW_STRATEGY_ERROR,"bp unwind attempted, but incoming bp(%p) was not"
	   " between sp (%p) and monitor stack bottom (%p)", 
	   bp, sp, monitor_stack_bottom());
      return STEP_ERROR;
    }
  }
  // bp relative
  next_sp  = (void **)((void *)bp + uw->bp_bp_pos);
  next_bp  = *next_sp;
  next_sp  = (void **)((void *)bp + uw->bp_ra_pos);
  void* ra_loc = (void*) next_sp;
  next_pc  = *next_sp;
  next_sp += 1;
  if ((void *)next_sp > sp) {
    // this condition is a weak correctness check. only
    // try building an interval for the return address again if it succeeds
    uw = (unwind_interval *)hpcrun_addr_to_interval(((char *)next_pc) - 1);
    if (! uw){
      if (((void *)next_sp) >= monitor_stack_bottom()) {
        TMSG(UNW_STRATEGY,"BP advance reaches monitor_stack_bottom,"
	     " next_sp = %p", next_sp);
        return STEP_STOP_WEAK;
      }
      TMSG(UNW_STRATEGY,"BP cannot build interval for next_pc(%p)", next_pc);
      return STEP_ERROR;
    }
    else {
      cursor->pc     = next_pc;
      cursor->bp     = next_bp;
      cursor->sp     = next_sp;
      cursor->ra_loc = ra_loc;

      cursor->intvl = (splay_interval_t *)uw;
      TMSG(UNW,"cursor advances ==>has_intvl=%d, bp=%p, sp=%p, pc=%p",
	   cursor->intvl != NULL, next_bp, next_sp, next_pc);
      return STEP_OK;
    }
  } else {
    TMSG(UNW_STRATEGY,"BP unwind fails: bp (%p) < sp (%p)", bp, sp);
    return STEP_ERROR;
  }
  EMSG("FALL Through BP unwind: shouldn't happen");
  return STEP_ERROR;
}


static step_state
unw_step_std(unw_cursor_t *cursor)
{
  int unw_res;

  if (unw_step_prefer_sp()){
    TMSG(UNW_STRATEGY,"--STD_FRAME: STARTing with SP");
    unw_res = unw_step_sp(cursor);
    if (unw_res == STEP_ERROR || unw_res == STEP_STOP_WEAK) {
      TMSG(UNW_STRATEGY,"--STD_FRAME: SP failed, RETRY w BP");
      unw_res = unw_step_bp(cursor);
      if (unw_res == STEP_STOP_WEAK) unw_res = STEP_STOP; 
    }
  } else {
    TMSG(UNW_STRATEGY,"--STD_FRAME: STARTing with BP");
    unw_res = unw_step_bp(cursor);
    if (unw_res == STEP_ERROR || unw_res == STEP_STOP_WEAK) {
      TMSG(UNW_STRATEGY,"--STD_FRAME: BP failed, RETRY w SP");
      unw_res = unw_step_sp(cursor);
    }
  }
  return unw_res;
}



// special steppers to artificially introduce error conditions
static step_state
t1_dbg_unw_step(unw_cursor_t *cursor)
{
  drop_sample();

  return STEP_ERROR;
}


static step_state GCC_ATTR_UNUSED
t2_dbg_unw_step(unw_cursor_t *cursor)
{
  static int s = 0;
  step_state rv;
  if (s == 0){
    update_cursor_with_troll(cursor, 1);
    rv = STEP_TROLL;
  }
  else {
    rv = STEP_STOP;
  }
  s =(s+1) %2;
  return rv;
}




//****************************************************************************
// private operations
//****************************************************************************

static void 
_drop_sample(bool no_backtrace)
{
  if (DEBUG_NO_LONGJMP) return;

  if (no_backtrace) {
    return;
  }
  if (hpcrun_below_pmsg_threshold()) {
    hpcrun_bt_dump(TD_GET(unwind), "DROP");
  }

  hpcrun_up_pmsg_count();

  sigjmp_buf_t *it = &(TD_GET(bad_unwind));
  (*hpcrun_get_real_siglongjmp())(it->jb, 9);
}


static void
drop_sample(void)
{
  _drop_sample(false);
}


static void
update_cursor_with_troll(unw_cursor_t *cursor, int offset)
{
  unsigned int tmp_ra_offset;

  int ret = stack_troll(cursor->sp, &tmp_ra_offset, &deep_validate_return_addr,
			(void *)cursor);

  if (ret != TROLL_INVALID) {
    void  **next_sp = ((void **)((unsigned long) cursor->sp + tmp_ra_offset));
    void *next_pc   = *next_sp;
    void *ra_loc    = (void*) next_sp;

    // the current base pointer is a good assumption for the caller's BP
    void **next_bp = (void **) cursor->bp; 

    next_sp += 1;
    if ( next_sp <= cursor->sp){
      PMSG_LIMIT(PMSG(TROLL,"Something weird happened! trolling from %p"
		      " resulted in sp not advancing", cursor->pc));
      hpcrun_unw_throw();
    }

    cursor->intvl = hpcrun_addr_to_interval(((char *)next_pc) + offset);
    if (cursor->intvl) {
      PMSG_LIMIT(PMSG(TROLL,"Trolling advances cursor to pc = %p, sp = %p", 
		      next_pc, next_sp));
      PMSG_LIMIT(PMSG(TROLL,"TROLL SUCCESS pc = %p", cursor->pc));

      cursor->pc     = next_pc;
      cursor->bp     = next_bp;
      cursor->sp     = next_sp;
      cursor->ra_loc = ra_loc;

      cursor->flags = 1; // trolling_used
      return; // success!
    }
    PMSG_LIMIT(PMSG(TROLL, "No interval found for trolled pc, dropping sample,"
		    " cursor pc = %p", cursor->pc));
    // fall through for error handling
  } else {
    PMSG_LIMIT(PMSG(TROLL, "Troll failed: dropping sample, cursor pc = %p", 
		    cursor->pc));
    PMSG_LIMIT(PMSG(TROLL,"TROLL FAILURE pc = %p", cursor->pc));
    // fall through for error handling
  }
  // assert(0);
  hpcrun_unw_throw();
}


static int 
hpcrun_check_fence(void *ip)
{
  return monitor_in_start_func_wide(ip);
}


//****************************************************************************
// debug operations
//****************************************************************************

static unw_cursor_t _dbg_cursor;

static void GCC_ATTR_UNUSED
dbg_init_cursor(void *context)
{
  DEBUG_NO_LONGJMP = 1;

  mcontext_t *mc = GET_MCONTEXT(context);

  _dbg_cursor.pc = MCONTEXT_PC(mc);
  _dbg_cursor.bp = MCONTEXT_BP(mc);
  _dbg_cursor.sp = MCONTEXT_SP(mc);

  DEBUG_NO_LONGJMP = 0;
}
