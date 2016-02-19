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

#define USE_LIBUNWIND 0


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
#include <unwind/common/unw-throw.h>
#include <unwind/common/fence_enum.h>
#include <fnbounds/fnbounds_interface.h>
#include "splay.h"
#include "ui_tree.h"
#include <utilities/arch/mcontext.h>
#include <utilities/ip-normalized.h>

#include <hpcrun/thread_data.h>
#include "x86-unwind-interval.h"
#include "x86-validate-retn-addr.h"

#if USE_LIBUNWIND
#include "x86-libunwind.h" 
#endif

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
update_cursor_with_troll(hpcrun_unw_cursor_t* cursor, int offset);

static fence_enum_t
hpcrun_check_fence(void* ip);

static void 
simulate_segv(void);

static void 
help_simulate_segv(bool no_backtrace);

static int
unw_step_prefer_sp(void);

static step_state 
unw_step_sp(hpcrun_unw_cursor_t* cursor);

static step_state
unw_step_bp(hpcrun_unw_cursor_t* cursor);

static step_state
unw_step_std(hpcrun_unw_cursor_t* cursor);

#if USE_LIBUNWIND
static step_state
unw_step_libunwind(hpcrun_unw_cursor_t* cursor);
#endif

static step_state
t1_dbg_unw_step(hpcrun_unw_cursor_t* cursor);

static step_state (*dbg_unw_step)(hpcrun_unw_cursor_t* cursor) = t1_dbg_unw_step;

extern void hpcrun_set_real_siglongjmp(void);

void
hpcrun_unw_init(void)
{
  x86_family_decoder_init();
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
  //
  // only implement 1 reg for the moment.
  //  add more if necessary
  //
  switch (reg_id) {
    case UNW_REG_IP:
      *reg_value = cursor->pc_unnorm;
      break;
    default:
      return ~0;
  }
  return 0;
}

static int
hpcrun_unw_get_norm_reg(hpcrun_unw_cursor_t* cursor, unw_reg_code_t reg_id, 
			ip_normalized_t* reg_value)
{
  //
  // only implement 1 reg for the moment.
  //  add more if necessary
  //
  switch (reg_id) {
    case UNW_REG_IP:
      *reg_value = cursor->pc_norm;
      break;
    default:
      return ~0;
  }
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


#if USE_LIBUNWIND
int 
hpcrun_unw_set_cursor(hpcrun_unw_cursor_t* cursor, void **sp, void **bp, void *ip)
{
  cursor->pc_unnorm = ip;
  cursor->bp 	    = bp;
  cursor->sp 	    = sp;
  cursor->ra_loc    = NULL;

  TMSG(UNW, "unw_set_cursor: pc=%p, ra_loc=%p, sp=%p, bp=%p", 
       cursor->pc_unnorm, cursor->ra_loc, cursor->sp, cursor->bp);

  cursor->flags = 0; // trolling_used
  cursor->intvl = hpcrun_addr_to_interval(cursor->pc_unnorm,
					  cursor->pc_unnorm, &cursor->pc_norm);
  cursor->pc_norm = hpcrun_normalize_ip(cursor->pc_unnorm, NULL);

  return (cursor->intvl != NULL);
}
#endif


void 
hpcrun_unw_init_cursor(hpcrun_unw_cursor_t* cursor, void* context)
{
  mcontext_t *mc = GET_MCONTEXT(context);

  cursor->pc_unnorm = MCONTEXT_PC(mc); 
  cursor->bp 	    = MCONTEXT_BP(mc);
  cursor->sp 	    = MCONTEXT_SP(mc);
  cursor->ra_loc    = NULL;

  TMSG(UNW, "unw_init: pc=%p, ra_loc=%p, sp=%p, bp=%p", 
       cursor->pc_unnorm, cursor->ra_loc, cursor->sp, cursor->bp);

  cursor->flags = 0; // trolling_used
  cursor->intvl = hpcrun_addr_to_interval(cursor->pc_unnorm,
					  cursor->pc_unnorm, &cursor->pc_norm);
  if (!cursor->intvl) {
    EMSG("unw_init: cursor could NOT build an interval for initial pc = %p",
	 cursor->pc_unnorm);
    cursor->pc_norm = hpcrun_normalize_ip(cursor->pc_unnorm, NULL);
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
hpcrun_unw_get_ra_loc(hpcrun_unw_cursor_t* cursor)
{
  return cursor->ra_loc;
}

static bool
fence_stop(fence_enum_t fence)
{
  return (fence == FENCE_MAIN) || (fence == FENCE_THREAD);
}

static step_state
hpcrun_unw_step_real(hpcrun_unw_cursor_t* cursor)
{

  cursor->fence = hpcrun_check_fence(cursor->pc_unnorm);

  //-----------------------------------------------------------
  // check if we have reached the end of our unwind, which is
  // demarcated with a fence. 
  //-----------------------------------------------------------
  if (fence_stop(cursor->fence)) {
    TMSG(UNW,"unw_step: STEP_STOP, current pc in monitor fence pc=%p\n", cursor->pc_unnorm);
    return STEP_STOP;
  }

  // current frame  
  void** bp = cursor->bp;
  void*  sp = cursor->sp;
  void*  pc = cursor->pc_unnorm;
  unwind_interval* uw = (unwind_interval *)cursor->intvl;

  int unw_res;

  if (!uw){
    TMSG(UNW, "unw_step: invalid unw interval for cursor, trolling ...");
    TMSG(TROLL, "Troll due to Invalid interval for pc %p", pc);
    update_cursor_with_troll(cursor, 0);
    return STEP_TROLL;
  }

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
    EMSG("unw_step: ILLEGAL UNWIND INTERVAL");
    dump_ui((unwind_interval *)cursor->intvl, 0);
    assert(0);
  }
  if (unw_res == STEP_STOP_WEAK) unw_res = STEP_STOP; 

  if (unw_res != STEP_ERROR) {
    return unw_res;
  }
  
  TMSG(TROLL,"unw_step: STEP_ERROR, pc=%p, bp=%p, sp=%p", pc, bp, sp);
  dump_ui_troll(uw);

  if (ENABLED(TROLL_WAIT)) {
    fprintf(stderr,"Hit troll point: attach w gdb to %d\n"
            "Maybe call dbg_set_flag(DBG_TROLL_WAIT,0) after attached\n",
	    getpid());
    
    // spin wait for developer to attach a debugger and clear the flag 
    while(DEBUG_WAIT_BEFORE_TROLLING);  
  }

#if USE_LIBUNWIND
  {
    hpcrun_unw_cursor_t libunwind_cursor = *cursor;
    unw_res = unw_step_libunwind(&libunwind_cursor);
  
    if (unw_res == STEP_STOP_WEAK) unw_res = STEP_STOP; 

    if (unw_res != STEP_ERROR) {
      *cursor = libunwind_cursor;
      return unw_res;
    }
  }

  show_backtrace();
#endif
  
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
hpcrun_unw_step(hpcrun_unw_cursor_t *cursor)
{
  if ( ENABLED(DBG_UNW_STEP) ){
    return dbg_unw_step(cursor);
  }
  
  hpcrun_unw_cursor_t saved = *cursor;
  step_state rv = hpcrun_unw_step_real(cursor);
  if ( ENABLED(UNW_VALID) ) {
    if (rv == STEP_OK) {
      // try to validate all calls, except the one at the base of the call stack from libmonitor.
      // rather than recording that as a valid call, it is preferable to ignore it.
      if (!monitor_in_start_func_wide(cursor->pc_unnorm)) {
	validation_status vstat = deep_validate_return_addr(cursor->pc_unnorm,
							    (void *) &saved);
	vrecord(saved.pc_unnorm,cursor->pc_unnorm,vstat);
      }
    }
  }
  return rv;
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
unw_step_sp(hpcrun_unw_cursor_t* cursor)
{
  TMSG(UNW_STRATEGY,"Using SP step");

  // void *stack_bottom = monitor_stack_bottom();

  // current frame
  void** bp = cursor->bp;
  void*  sp = cursor->sp;
  void*  pc = cursor->pc_unnorm;
  unwind_interval* uw = (unwind_interval *)cursor->intvl;
  
  TMSG(UNW,"step_sp: cursor { bp=%p, sp=%p, pc=%p }", bp, sp, pc);
  if (MYDBG) { dump_ui(uw, 0); }

  void** next_bp = NULL;
  void** next_sp = (void **)(sp + uw->sp_ra_pos);
  void*  ra_loc  = (void*) next_sp;
  void*  next_pc  = *next_sp;

  TMSG(UNW,"  step_sp: potential next cursor next_sp=%p ==> next_pc = %p",
       next_sp, next_pc);

  if (uw->bp_status == BP_UNCHANGED){
    next_bp = bp;
    TMSG(UNW,"  step_sp: unwind step has BP_UNCHANGED ==> next_bp=%p", next_bp);
  }
  else {
    //-----------------------------------------------------------
    // reload the candidate value for the caller's BP from the 
    // save area in the activation frame according to the unwind 
    // information produced by binary analysis
    //-----------------------------------------------------------
    next_bp = (void **)(sp + uw->sp_bp_pos);
    TMSG(UNW,"  step_sp: unwind next_bp loc = %p", next_bp);
    next_bp  = *next_bp; 
    TMSG(UNW,"  step_sp: sp unwind next_bp val = %p", next_bp);

  }
  next_sp += 1;
  ip_normalized_t next_pc_norm = ip_normalized_NULL;
  cursor->intvl = hpcrun_addr_to_interval(((char *)next_pc) - 1,
					  next_pc, &next_pc_norm);

  if (! cursor->intvl){
    if (((void *)next_sp) >= monitor_stack_bottom()){
      TMSG(UNW,"  step_sp: STEP_STOP_WEAK, no next interval and next_sp >= stack bottom,"
	   " so stop unwind ...");
      return STEP_STOP_WEAK;
    }
    else {
      TMSG(UNW,"  sp STEP_ERROR: no next interval, step fails");
      return STEP_ERROR;
    }
  }
  else {
    // sanity check to avoid infinite unwind loop
    if (next_sp <= cursor->sp){
      TMSG(INTV_ERR,"@ pc = %p. sp unwind does not advance stack." 
	   " New sp = %p, old sp = %p", cursor->pc_unnorm, next_sp,
	   cursor->sp);
      
      return STEP_ERROR;
    }
    unwind_interval* uw = (unwind_interval *)cursor->intvl;
    if ((RA_BP_FRAME == uw->ra_status) ||
	(RA_STD_FRAME == uw->ra_status)) { // Makes sense to sanity check BP, do it
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
	TMSG(UNW,"  step_sp: unwind bp sanity check fails."
	     " Resetting next_bp to current bp = %p", next_bp);
      }
    }
    cursor->pc_unnorm = next_pc;
    cursor->bp 	      = next_bp;
    cursor->sp 	      = next_sp;
    cursor->ra_loc    = ra_loc;
    cursor->pc_norm   = next_pc_norm;
  }

  TMSG(UNW,"  step_sp: STEP_OK, has_intvl=%d, bp=%p, sp=%p, pc=%p",
	   cursor->intvl != NULL, next_bp, next_sp, next_pc);
  return STEP_OK;
}


static step_state
unw_step_bp(hpcrun_unw_cursor_t* cursor)
{
  void *sp, **bp, *pc; 
  void **next_sp, **next_bp, *next_pc;

  unwind_interval *uw;

  TMSG(UNW_STRATEGY,"Using BP step");
  // current frame
  bp = cursor->bp;
  sp = cursor->sp;
  pc = cursor->pc_unnorm;
  uw = (unwind_interval *)cursor->intvl;

  TMSG(UNW,"step_bp: cursor { bp=%p, sp=%p, pc=%p }", bp, sp, pc);
  if (MYDBG) { dump_ui(uw, 0); }

  if (!(sp <= (void*) bp)) {
    TMSG(UNW,"  step_bp: STEP_ERROR, unwind attempted, but incoming bp(%p) was not"
	 " >= sp(%p)", bp, sp);
    return STEP_ERROR;
  }
  if (DISABLED(OMP_SKIP_MSB)) {
    if (!((void *)bp < monitor_stack_bottom())) {
      TMSG(UNW,"  step_bp: STEP_ERROR, unwind attempted, but incoming bp(%p) was not"
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
    ip_normalized_t next_pc_norm = ip_normalized_NULL;
    uw = (unwind_interval *)hpcrun_addr_to_interval(((char *)next_pc) - 1, 
						    next_pc, &next_pc_norm);
    if (! uw){
      if (((void *)next_sp) >= monitor_stack_bottom()) {
        TMSG(UNW,"  step_bp: STEP_STOP_WEAK, next_sp >= monitor_stack_bottom,"
	     " next_sp = %p", next_sp);
        return STEP_STOP_WEAK;
      }
      TMSG(UNW,"  step_bp: STEP_ERROR, cannot build interval for next_pc(%p)", next_pc);
      return STEP_ERROR;
    }
    else {
      cursor->pc_unnorm = next_pc;
      cursor->bp        = next_bp;
      cursor->sp        = next_sp;
      cursor->ra_loc    = ra_loc;
      cursor->pc_norm   = next_pc_norm;
      
      cursor->intvl = (splay_interval_t *)uw;
      TMSG(UNW,"  step_bp: STEP_OK, has_intvl=%d, bp=%p, sp=%p, pc=%p",
	   cursor->intvl != NULL, next_bp, next_sp, next_pc);
      return STEP_OK;
    }
  }
  else {
    TMSG(UNW_STRATEGY,"BP unwind fails: bp (%p) < sp (%p)", bp, sp);
    return STEP_ERROR;
  }
  EMSG("FALL Through BP unwind: shouldn't happen");
  return STEP_ERROR;
}


#if USE_LIBUNWIND
static step_state
unw_step_libunwind(hpcrun_unw_cursor_t* cursor)
{
  void **sp = cursor->sp;
  void **bp = cursor->bp;
  void *ip = cursor->pc_unnorm;

  int libuw_success = libunwind_step(&sp, &bp, &ip);
  if (libuw_success) {
    // libunwind was successful; update cursor based on its results 
    int success = hpcrun_unw_set_cursor(cursor, sp, bp, ip);

    if (success) {
      return STEP_OK;
    } else {
      void *next_sp =  cursor->sp;
      void *next_pc = cursor->pc_unnorm;
      if (next_sp >= monitor_stack_bottom()) {
        TMSG(UNW,"  step_libunwind: STEP_STOP_WEAK, " 
             "next_sp >= monitor_stack_bottom, next_sp = %p", next_sp);
        return STEP_STOP_WEAK;
      }
      TMSG(UNW,"  step_libunwind: STEP_ERROR, cannot build interval " 
           "for next_pc(%p)", next_pc);
    }
  }
  return STEP_ERROR;
}
#endif


static step_state
unw_step_std(hpcrun_unw_cursor_t* cursor)
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
  }
  else {
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
t1_dbg_unw_step(hpcrun_unw_cursor_t* cursor)
{
  simulate_segv();

  return STEP_ERROR;
}


static step_state GCC_ATTR_UNUSED
t2_dbg_unw_step(hpcrun_unw_cursor_t* cursor)
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
help_simulate_segv(bool no_backtrace)
{
  if (DEBUG_NO_LONGJMP) return;

  if (no_backtrace) {
    return;
  }
  if (hpcrun_below_pmsg_threshold()) {
    hpcrun_bt_dump(TD_GET(btbuf_cur), "DROP");
  }

  hpcrun_up_pmsg_count();

  sigjmp_buf_t *it = &(TD_GET(bad_unwind));
  (*hpcrun_get_real_siglongjmp())(it->jb, 9);
}


static void
simulate_segv(void)
{
  help_simulate_segv(false);
}


static void
update_cursor_with_troll(hpcrun_unw_cursor_t* cursor, int offset)
{
  if (ENABLED(NO_TROLLING)){
    TMSG(TROLL, "Trolling disabled");
    hpcrun_unw_throw();
  }

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
      TMSG(TROLL,"Something weird happened! trolling from %p"
	   " resulted in sp not advancing", cursor->pc_unnorm);
      hpcrun_unw_throw();
    }

    ip_normalized_t next_pc_norm = ip_normalized_NULL;
    cursor->intvl = hpcrun_addr_to_interval(((char *)next_pc) + offset,
					    next_pc, &next_pc_norm);
    if (cursor->intvl) {
      TMSG(TROLL,"Trolling advances cursor to pc = %p, sp = %p", 
	   next_pc, next_sp);
      TMSG(TROLL,"TROLL SUCCESS pc = %p", cursor->pc_unnorm);

      cursor->pc_unnorm = next_pc;
      cursor->bp        = next_bp;
      cursor->sp        = next_sp;
      cursor->ra_loc    = ra_loc;
      cursor->pc_norm   = next_pc_norm;

      cursor->flags = 1; // trolling_used

      return; // success!
    }
    TMSG(TROLL, "No interval found for trolled pc, dropping sample,"
	 " cursor pc = %p", cursor->pc_unnorm);
    // fall through for error handling
  }
  else {
    TMSG(TROLL, "Troll failed: dropping sample, cursor pc = %p", 
	 cursor->pc_unnorm);
    TMSG(TROLL,"TROLL FAILURE pc = %p", cursor->pc_unnorm);
    // fall through for error handling
  }
  // assert(0);
  hpcrun_unw_throw();
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

//****************************************************************************
// debug operations
//****************************************************************************

static hpcrun_unw_cursor_t _dbg_cursor;

static void GCC_ATTR_UNUSED
dbg_init_cursor(void* context)
{
  DEBUG_NO_LONGJMP = 1;

  mcontext_t *mc = GET_MCONTEXT(context);

  _dbg_cursor.pc_unnorm = MCONTEXT_PC(mc);
  _dbg_cursor.bp        = MCONTEXT_BP(mc);
  _dbg_cursor.sp        = MCONTEXT_SP(mc);
  _dbg_cursor.pc_norm = hpcrun_normalize_ip(_dbg_cursor.pc_unnorm, NULL);

  DEBUG_NO_LONGJMP = 0;
}
