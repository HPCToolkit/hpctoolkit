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
// Copyright ((c)) 2002-2018, Rice University
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
// external includes
//***************************************************************************

#include <monitor.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

//***************************************************************************
// local include files 
//***************************************************************************

#include <include/gcc-attr.h>
#include <x86-decoder.h>

#include <hpcrun/epoch.h>
#include <hpcrun/main.h>
#include "stack_troll.h"
#include "thread_use.h"

#include <unwind/common/unwind.h>
#include <unwind/common/backtrace.h>
#include <unwind/common/libunw_intervals.h>
#include <unwind/common/unw-throw.h>
#include <unwind/common/validate_return_addr.h>
#include <unwind/common/fence_enum.h>
#include <fnbounds/fnbounds_interface.h>
#include <unwind/common/uw_recipe_map.h>
#include <utilities/arch/mcontext.h>
#include <utilities/ip-normalized.h>

#include <hpcrun/main.h>
#include <hpcrun/thread_data.h>
#include "x86-build-intervals.h"
#include "x86-unwind-interval.h"
#include "x86-validate-retn-addr.h"

#include <messages/messages.h>
#include <messages/debug-flag.h>


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

static step_state 
unw_step_sp(hpcrun_unw_cursor_t* cursor);

static step_state
unw_step_bp(hpcrun_unw_cursor_t* cursor);

static step_state
unw_step_std(hpcrun_unw_cursor_t* cursor);

static step_state
t1_dbg_unw_step(hpcrun_unw_cursor_t* cursor);

static step_state (*dbg_unw_step)(hpcrun_unw_cursor_t* cursor) = t1_dbg_unw_step;

//************************************************
// private functions
//************************************************

static void
save_registers(hpcrun_unw_cursor_t* cursor, void *pc, void *bp, void *sp,
	       void **ra_loc)
{
  cursor->pc_unnorm = pc;
  cursor->bp        = bp;
  cursor->sp        = sp;
  cursor->ra_loc    = ra_loc;
}

static void
compute_normalized_ips(hpcrun_unw_cursor_t* cursor)
{
  void *func_start_pc =  (void*) cursor->unwr_info.interval.start;
  load_module_t* lm = cursor->unwr_info.lm;

  cursor->pc_norm = hpcrun_normalize_ip(cursor->pc_unnorm, lm);
  cursor->the_function = hpcrun_normalize_ip(func_start_pc, lm);
}



//************************************************
// interface functions
//************************************************

void
hpcrun_unw_init(void)
{
  x86_family_decoder_init();
  uw_recipe_map_init();
}

typedef unw_frame_regnum_t unw_reg_code_t;

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
hpcrun_unw_get_ip_unnorm_reg(hpcrun_unw_cursor_t* c, void** reg_value)
{
  return hpcrun_unw_get_unnorm_reg(c, UNW_REG_IP, reg_value);
}

void 
hpcrun_unw_init_cursor(hpcrun_unw_cursor_t* cursor, void* context)
{
  libunw_unw_init_cursor(cursor, context);

  void *pc, **bp, *sp;
  unw_get_reg(&cursor->uc, UNW_REG_IP, (unw_word_t *)&pc);
  unw_get_reg(&cursor->uc, UNW_REG_SP, (unw_word_t *)&sp);
  unw_get_reg(&cursor->uc, UNW_TDEP_BP, (unw_word_t *)&bp);
  save_registers(cursor, pc, bp, sp, NULL);

  if (cursor->libunw_status == LIBUNW_OK)
    return;

  bool found = uw_recipe_map_lookup(pc, NATIVE_UNWINDER, &cursor->unwr_info);

  if (!found) {
    EMSG("unw_init: cursor could NOT build an interval for initial pc = %p",
	 cursor->pc_unnorm);
  }

  compute_normalized_ips(cursor);

  if (MYDBG) { dump_ui(cursor->unwr_info.btuwi, 0); }
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

btuwi_status_t
build_intervals(char *ins, unsigned int len, unwinder_t uw)
{
  if (uw == NATIVE_UNWINDER)
    return x86_build_intervals(ins, len, 0);
  return libunw_build_intervals(ins, len);
}


static step_state
hpcrun_unw_step_real(hpcrun_unw_cursor_t* cursor)
{
  void *pc = cursor->pc_unnorm;
  cursor->fence = (monitor_unwind_process_bottom_frame(pc) ? FENCE_MAIN :
		   monitor_unwind_thread_bottom_frame(pc)? FENCE_THREAD : FENCE_NONE);
		   
  if (cursor->fence != FENCE_NONE) {
    if (ENABLED(FENCE_UNW))
      TMSG(FENCE_UNW, "%s", fence_enum_name(cursor->fence));

    //-----------------------------------------------------------
    // check if we have reached the end of our unwind, which is
    // demarcated with a fence. 
    //-----------------------------------------------------------
    TMSG(UNW,"unw_step: STEP_STOP, current pc in monitor fence pc=%p\n", pc);
    return STEP_STOP;
  }

  // current frame  
  void** bp = cursor->bp;
  void*  sp = cursor->sp;
  unwind_interval* uw = cursor->unwr_info.btuwi;

  if (!uw) {
    TMSG(UNW, "unw_step: invalid unw interval for cursor, trolling ...");
    TMSG(TROLL, "Troll due to Invalid interval for pc %p", pc);
    update_cursor_with_troll(cursor, 0);
    return STEP_TROLL;
  }

  step_state unw_res;
  switch (UWI_RECIPE(uw)->ra_status) {
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
    dump_ui(cursor->unwr_info.btuwi, 0);
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

  update_cursor_with_troll(cursor, 1);

  return STEP_TROLL;
}

size_t hpcrun_validation_counts[] = {
#define _MM(a) [UNW_ADDR_ ## a] = 0,
VSTAT_ENUMS
#undef _MM
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


static step_state
hpcrun_retry_libunw_find_step(hpcrun_unw_cursor_t *cursor,
			      void *pc, void **sp, void **bp)
{
  unw_context_t uc;
  memcpy(&uc, &cursor->uc, sizeof(uc));
  LV_MCONTEXT_PC(&uc.uc_mcontext) = (intptr_t)pc;
  LV_MCONTEXT_SP(&uc.uc_mcontext) = (intptr_t)sp;
  LV_MCONTEXT_BP(&uc.uc_mcontext) = (intptr_t)bp;
  unw_init_local(&cursor->uc, &uc);
  cursor->libunw_status = LIBUNW_OK;
  return (libunw_find_step(cursor));
}

step_state
hpcrun_unw_step(hpcrun_unw_cursor_t *cursor)
{
  step_state unw_res;

  if (cursor->libunw_status == LIBUNW_OK) {
    unw_res = libunw_take_step(cursor);

    void *pc, **bp, *sp;
    unw_get_reg(&cursor->uc, UNW_REG_IP, (unw_word_t *)&pc);
    unw_get_reg(&cursor->uc, UNW_REG_SP, (unw_word_t *)&sp);
    unw_get_reg(&cursor->uc, UNW_TDEP_BP, (unw_word_t *)&bp);
    save_registers(cursor, pc, bp, sp, (void *)(sp - 1));

    if (unw_res == STEP_STOP)
      return (STEP_STOP);

    if (libunw_find_step(cursor) == STEP_OK)
      return (STEP_OK);
  }

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

static step_state
unw_step_sp(hpcrun_unw_cursor_t* cursor)
{
  TMSG(UNW_STRATEGY,"Using SP step");

  // void *stack_bottom = monitor_stack_bottom();

  // current frame
  void** bp = cursor->bp;
  void*  sp = cursor->sp;
  void*  pc = cursor->pc_unnorm;
  unwind_interval* uw = cursor->unwr_info.btuwi;
  x86recipe_t *xr = UWI_RECIPE(uw);

  if (xr == NULL) {
    return STEP_ERROR;
  }
  
  TMSG(UNW,"step_sp: cursor { bp=%p, sp=%p, pc=%p }", bp, sp, pc);
  if (MYDBG) { dump_ui(uw, 0); }

  void** next_bp = xr->reg.bp_status == BP_UNCHANGED ? bp :
    //-----------------------------------------------------------
    // reload the candidate value for the caller's BP from the 
    // save area in the activation frame according to the unwind 
    // information produced by binary analysis
    //-----------------------------------------------------------
    *(void **)(sp + xr->reg.sp_bp_pos);
  void** next_sp = (void **)(sp + xr->reg.sp_ra_pos);
  void*  ra_loc  = (void*) next_sp;
  void*  next_pc  = *next_sp++;

  if ((RA_BP_FRAME == xr->ra_status) ||
      (RA_STD_FRAME == xr->ra_status)) { // Makes sense to sanity check BP, do it
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

  if (hpcrun_retry_libunw_find_step(cursor, next_pc, next_sp, next_bp) == STEP_OK)
    return STEP_OK;


  TMSG(UNW,"  step_sp: potential next cursor next_sp=%p ==> next_pc = %p",
       next_sp, next_pc);

  unwindr_info_t unwr_info;
  bool found = uw_recipe_map_lookup(((char *)next_pc) - 1, NATIVE_UNWINDER, &unwr_info);
  if (!found){
    if (((void *)next_sp) >= monitor_stack_bottom()){
      TMSG(UNW,"  step_sp: STEP_STOP_WEAK, no next interval and next_sp >= stack bottom,"
	   " so stop unwind ...");
      return STEP_STOP_WEAK;
    }
    TMSG(UNW,"  sp STEP_ERROR: no next interval, step fails");
    return STEP_ERROR;
  }

  // sanity check to avoid infinite unwind loop
  if (next_sp <= cursor->sp){
    TMSG(INTV_ERR,"@ pc = %p. sp unwind does not advance stack." 
	 " New sp = %p, old sp = %p", cursor->pc_unnorm, next_sp,
	 cursor->sp);
      
    return STEP_ERROR;
  }
  TMSG(UNW,"  step_sp: STEP_OK, has_intvl=%d, bp=%p, sp=%p, pc=%p",
       cursor->unwr_info.btuwi != NULL, next_bp, next_sp, next_pc);

  cursor->unwr_info = unwr_info;
  save_registers(cursor, next_pc, next_bp, next_sp, ra_loc);
  compute_normalized_ips(cursor);
  return STEP_OK;
}


static step_state
unw_step_bp(hpcrun_unw_cursor_t* cursor)
{
  TMSG(UNW_STRATEGY,"Using BP step");
  // current frame
  void **bp = cursor->bp;
  void *sp = cursor->sp;
  void *pc = cursor->pc_unnorm;
  unwind_interval *uw = cursor->unwr_info.btuwi;
  x86recipe_t *xr = UWI_RECIPE(uw);

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
  void **next_bp  = *(void **)((void *)bp + xr->reg.bp_bp_pos);
  void **next_sp  = (void **)((void *)bp + xr->reg.bp_ra_pos);
  void* ra_loc = (void*) next_sp;
  void *next_pc  = *next_sp++;

  
  if (hpcrun_retry_libunw_find_step(cursor, next_pc, next_sp, next_bp) == STEP_OK)
    return STEP_OK;

  // this condition is a weak correctness check. only
  // try building an interval for the return address again if it succeeds
  if ((void *)next_sp <= sp) {
    TMSG(UNW_STRATEGY,"BP unwind fails: bp (%p) < sp (%p)", bp, sp);
    return STEP_ERROR;
  }

  unwindr_info_t unwr_info;
  bool found = uw_recipe_map_lookup(((char *)next_pc) - 1, NATIVE_UNWINDER, &unwr_info);
  if (!found){
    if (((void *)next_sp) >= monitor_stack_bottom()) {
      TMSG(UNW,"  step_bp: STEP_STOP_WEAK, next_sp >= monitor_stack_bottom,"
	   " next_sp = %p", next_sp);
      return STEP_STOP_WEAK;
    }
    TMSG(UNW,"  step_bp: STEP_ERROR, cannot build interval for next_pc(%p)", next_pc);
    return STEP_ERROR;
  }
  TMSG(UNW,"  step_bp: STEP_OK, has_intvl=%d, bp=%p, sp=%p, pc=%p",
       unwr_info.btuwi != NULL, next_bp, next_sp, next_pc);
  assert(ra_loc == (void *)(next_sp - 1));
  cursor->unwr_info = unwr_info;
  save_registers(cursor, next_pc, next_bp, next_sp, ra_loc);
  compute_normalized_ips(cursor);

  return STEP_OK;
}

static step_state
unw_step_std(hpcrun_unw_cursor_t* cursor)
{
  int unw_res;

  if (ENABLED(PREFER_SP)) {
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
  if (!DEBUG_NO_LONGJMP) {

    if (hpcrun_below_pmsg_threshold()) {
      hpcrun_bt_dump(TD_GET(btbuf_cur), "DROP");
    }

    hpcrun_up_pmsg_count();

    sigjmp_buf_t *it = &(TD_GET(bad_unwind));
    (*hpcrun_get_real_siglongjmp())(it->jb, 9);
  }

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

    bool found = uw_recipe_map_lookup(((char *)next_pc) + offset, NATIVE_UNWINDER, &(cursor->unwr_info));
    if (found) {
      TMSG(TROLL,"Trolling advances cursor to pc = %p, sp = %p", 
	   next_pc, next_sp);
      TMSG(TROLL,"TROLL SUCCESS pc = %p", cursor->pc_unnorm);

      assert(ra_loc == (void *)(next_sp - 1));
      save_registers(cursor, next_pc, next_bp, next_sp, ra_loc);
      compute_normalized_ips(cursor);
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
