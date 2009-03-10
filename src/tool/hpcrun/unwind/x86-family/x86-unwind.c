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

#include "state.h"
#include "general.h"
#include "mem.h"
#include "pmsg.h"
#include "stack_troll.h"

#include "unwind.h"
#include "backtrace.h"
#include "splay.h"
#include "ui_tree.h"

#include "thread_data.h"
#include "x86-unwind-interval.h"
#include "validate_return_addr.h"

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

static void update_cursor_with_troll(unw_cursor_t *cursor, int offset);

static int csprof_check_fence(void *ip);

static int unw_step_prefer_sp(void);

static void drop_sample(void);

static validation_status validate_return_addr(void *addr, void *generic_arg);
static validation_status simple_validate_addr(void *addr, void *generic_arg);


// tallent: FIXME: obsolete
void unw_init_arch(void);
void unw_init_cursor_arch(void* context, unw_cursor_t *cursor);
int  unw_get_reg_arch(unw_cursor_t *c, int reg_id, void **reg_value);


//****************************************************************************
// interface functions
//****************************************************************************

void
unw_init(void)
{
  unw_init_arch();
  csprof_interval_tree_init();
}


void 
unw_init_cursor(unw_cursor_t* cursor, void* context)
{
  extern void dump_ui(unwind_interval *u, int dump_to_stdout);

#if 0
  TMSG(UNW,"init prim unw called with context = %p, cursor_p = %p\n",
       context, cursor);
#endif

  unw_init_cursor_arch(context, cursor);

  TMSG(UNW,"INIT CURSOR: frame pc = %p, frame bp = %p, frame sp = %p", 
       cursor->pc, cursor->bp, cursor->sp);

  cursor->flags = 0; // trolling_used
  cursor->intvl = csprof_addr_to_interval(cursor->pc);

  if (!cursor->intvl) {
    TMSG(TROLL,"UNW INIT FAILURE: frame pc = %p, frame bp = %p, frame sp = %p",
         cursor->pc, cursor->bp, cursor->sp);

    TMSG(TROLL,"UNW INIT calls stack troll");

    update_cursor_with_troll(cursor, 0);
  } 

  TMSG(UNW,"dumping the found interval");
  dump_ui((unwind_interval *)cursor->intvl, 0); // debug for now
}


int 
unw_get_reg(unw_cursor_t *cursor, int reg_id, void **reg_value)
{
   unw_get_reg_arch(cursor, reg_id, reg_value);
   return 0;
}


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


step_state
unw_step_sp(unw_cursor_t *cursor)
{
  void *sp, **bp, *pc; 
  void **next_sp, **next_bp, *next_pc;

  unwind_interval *uw;

  TMSG(UNW_STRATEGY,"Using SP step");

  void *stack_bottom = monitor_stack_bottom();

  // current frame
  bp = cursor->bp;
  sp = cursor->sp;
  pc = cursor->pc;
  uw = (unwind_interval *)cursor->intvl;
  TMSG(UNW,"cursor in ==> bp=%p, sp=%p, pc=%p", bp, sp, pc);
  TMSG(UNW,"unwind interval in below:");
  dump_ui(uw, 0);

  next_sp  = (void **)(sp + uw->sp_ra_pos);
#if 0
  if (! (sp <= (void *)next_sp && (void *)next_sp < stack_bottom)) {
    TMSG(UNW,"sp unwind step gave invalid next sp. cursor sp = %p,"
	 " next_sp = %p", sp, next_sp);
    return STEP_ERROR;
  }
#endif
  next_pc  = *next_sp;

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
  cursor->intvl = csprof_addr_to_interval(((char *)next_pc) - 1);

  if (! cursor->intvl){
    if (((void *)next_sp) >= monitor_stack_bottom()){
      TMSG(UNW,"No next interval, and next_sp >= stack bottom,"
	   " so stop unwind ...");
      return STEP_STOP;
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
    cursor->pc = next_pc;
    cursor->bp = next_bp;
    cursor->sp = next_sp;
  }

#if 0
  TMSG(UNW,"dumping the found interval");
  dump_ui((unwind_interval *)cursor->intvl,1); // debug for now
  TMSG(UNW,"NEXT frame pc = %p, frame bp = %p\n", cursor->pc, cursor->bp);
#endif

  TMSG(UNW,"==== sp advance ok ===");
  return STEP_OK;
}


step_state
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
  dump_ui(uw, 0);

  if (!(sp <= (void *)bp && (void *)bp < monitor_stack_bottom())) {
    TMSG(UNW_STRATEGY_ERROR,"bp unwind attempted, but incoming bp(%p) was not"
         " between sp (%p) and monitor stack bottom (%p)", 
         bp, sp, monitor_stack_bottom());
    return STEP_ERROR;
  }

  // bp relative
  next_sp  = (void **)((void *)bp + uw->bp_bp_pos);
  next_bp  = *next_sp;
  next_sp  = (void **)((void *)bp + uw->bp_ra_pos);
  next_pc  = *next_sp;
  next_sp += 1;
  if ((void *)next_sp > sp) {
    // this condition is a weak correctness check. only
    // try building an interval for the return address again if it succeeds
    uw = (unwind_interval *)csprof_addr_to_interval(((char *)next_pc) - 1);
    if (! uw){
      if (((void *)next_sp) >= monitor_stack_bottom()) {
        TMSG(UNW_STRATEGY,"BP advance reaches monitor_stack_bottom,"
	     " next_sp = %p", next_sp);
        return STEP_STOP;
      }
      TMSG(UNW_STRATEGY,"BP cannot build interval for next_pc(%p)", next_pc);
      return STEP_ERROR;
    } else {
      cursor->pc    = next_pc;
      cursor->bp    = next_bp;
      cursor->sp    = next_sp;
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


step_state
unw_step_std(unw_cursor_t *cursor)
{
  int unw_res;

  if (unw_step_prefer_sp()){
    TMSG(UNW_STRATEGY,"--STD_FRAME: STARTing with SP");
    unw_res = unw_step_sp(cursor);
    if (unw_res == STEP_ERROR) {
      TMSG(UNW_STRATEGY,"--STD_FRAME: SP failed, RETRY w BP");
      unw_res = unw_step_bp(cursor);
    }
  } else {
    TMSG(UNW_STRATEGY,"--STD_FRAME: STARTing with BP");
    unw_res = unw_step_bp(cursor);
    if (unw_res == STEP_ERROR) {
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


static step_state (*dbg_unw_step)(unw_cursor_t *cursor) = t1_dbg_unw_step;

extern int samples_taken;


step_state
unw_step (unw_cursor_t *cursor)
{
  if ( ENABLED(DBG_UNW_STEP) ){
    return dbg_unw_step(cursor);
  }

  //-----------------------------------------------------------
  // check if we have reached the end of our unwind, which is
  // demarcated with a fence. 
  //-----------------------------------------------------------
  if (csprof_check_fence(cursor->pc)){
    TMSG(UNW,"current pc in monitor fence", cursor->pc);
    return STEP_STOP;
  }

  void *sp, **bp, *pc; 

  unwind_interval *uw;

  // current frame
  bp = cursor->bp;
  sp = cursor->sp;
  pc = cursor->pc;
  uw = (unwind_interval *)cursor->intvl;

  int unw_res;

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
    EMSG("ILLEGAL UNWIND INTERVAL");
    dump_ui((unwind_interval *)cursor->intvl, 0); // debug for now
    assert(0);
  }
  if (unw_res != -1){
    TMSG(UNW,"=========== unw_step Succeeds ============== ");
    return unw_res;
  }
  PMSG_LIMIT(TMSG(TROLL,"UNW STEP FAILURE : cursor pc = %p, cursor bp = %p,"
		  " cursor sp = %p", pc, bp, sp));
  PMSG_LIMIT(TMSG(TROLL,"UNW STEP calls stack troll"));

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

// public interface to local drop sample
void
unw_throw(void)
{
  drop_sample();
}



//****************************************************************************
// private operations
//****************************************************************************

static void 
drop_sample(void)
{
  if (DEBUG_NO_LONGJMP) return;

  if (csprof_below_pmsg_threshold()) {
    dump_backtrace(TD_GET(state),0);
  }

  csprof_up_pmsg_count();

  sigjmp_buf_t *it = &(TD_GET(bad_unwind));
  siglongjmp(it->jb,9);
}


// TODO: build up appropriate generic argument

static void 
update_cursor_with_troll(unw_cursor_t *cursor, int offset)
{
  unsigned int tmp_ra_offset;

#if 0
  int ret = stack_troll(cursor->sp, &tmp_ra_offset, &validate_return_addr, 
			(void *)cursor);
#else
  int ret = stack_troll(cursor->sp, &tmp_ra_offset, &simple_validate_addr, 
			(void *)cursor);
#endif

  if (ret != TROLL_INVALID) {
    void  **next_sp = ((void **)((unsigned long) cursor->sp + tmp_ra_offset));
    void *next_pc = *next_sp;

    // the current base pointer is a good assumption for the caller's BP
    void **next_bp = (void **) cursor->bp; 

    next_sp += 1;
    if ( next_sp <= cursor->sp){
      PMSG_LIMIT(PMSG(TROLL,"Something weird happened! trolling from %p"
		      " resulted in sp not advancing", cursor->pc));
      drop_sample();
    }

    cursor->intvl = csprof_addr_to_interval(((char *)next_pc) + offset);
    if (cursor->intvl) {
      PMSG_LIMIT(PMSG(TROLL,"Trolling advances cursor to pc = %p, sp = %p", 
		      next_pc, next_sp));
      PMSG_LIMIT(PMSG(TROLL,"TROLL SUCCESS pc = %p", cursor->pc));

      cursor->pc = next_pc;
      cursor->bp = next_bp;
      cursor->sp = next_sp;

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
  drop_sample();
}


#include "validate_return_addr.h"
#include "fnbounds_interface.h"

/* static */ void *
vget_branch_target(void *ins, xed_decoded_inst_t *xptr, 
		   xed_operand_values_t *vals)
{
  int bytes = xed_operand_values_get_branch_displacement_length(vals);
  int offset = 0;
  switch(bytes) {
  case 1:
    offset = (signed char) 
      xed_operand_values_get_branch_displacement_byte(vals,0);
    break;
  case 4:
    offset = xed_operand_values_get_branch_displacement_int32(vals);
    break;
  default:
    assert(0);
  }
  void *end_of_call_inst = ins + xed_decoded_inst_get_length(xptr);
  void *target = end_of_call_inst + offset;
  return target;
}

/* static */ void *
vdecode_call(void *ins, xed_decoded_inst_t *xptr)
{
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_type_enum_t op0_type = xed_operand_type(op0);

  if (op0_name == XED_OPERAND_RELBR && 
      op0_type == XED_OPERAND_TYPE_IMM_CONST) {
    // fprintf(stderr,"looks like constant call\n");
    xed_operand_values_t *vals = xed_decoded_inst_operands(xptr);

    if (xed_operand_values_has_branch_displacement(vals)) {
      void *vaddr = vget_branch_target(ins, xptr, vals);
      // fprintf(stderr,"apparent call to %p\n", vaddr);
      return vaddr;
    }
  }
  return NULL;
}


/* static */ bool
confirm_call(void *addr, void *routine)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;
  xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);
  xed_decoded_inst_zero_keep_mode(xptr);
  void *possible_call = addr - 5;
  xed_error = xed_decode(xptr, (uint8_t *)possible_call, 15);

  TMSG(VALIDATE_UNW,"trying to confirm a call from return addr %p", addr);

  if (xed_error != XED_ERROR_NONE) {
    TMSG(VALIDATE_UNW, "addr %p has XED decode error when attempting confirm",
	 possible_call);
    return false;
  }

  xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);
  switch(xiclass) {
    case XED_ICLASS_CALL_FAR:
    case XED_ICLASS_CALL_NEAR:
      TMSG(VALIDATE_UNW,"call instruction confirmed @ %p", possible_call);
      void *the_call = vdecode_call(possible_call, xptr);
      TMSG(VALIDATE_UNW,"comparing discovered call %p to actual routine %p",
	   the_call, routine);
      return (the_call == routine);
      break;
    default:
      return false;
  }
  EMSG("confirm call shouldn't get here!");
  return false;
}


static bool
indirect_or_tail(void *addr, void *my_routine)
{
  TMSG(VALIDATE_UNW,"checking for indirect or tail call");
  return true;
}


static validation_status
validate_return_addr(void *addr, void *generic)
{
  unw_cursor_t *cursor = (unw_cursor_t *)generic;

  void *beg, *end;
  if (fnbounds_enclosing_addr(addr, &beg, &end)) {
    return UNW_ADDR_WRONG;
  }

  void *my_routine = cursor->pc;
  if (fnbounds_enclosing_addr(my_routine,&beg,&end)) {
    return UNW_ADDR_WRONG;
  }
  my_routine = beg;
  TMSG(VALIDATE_UNW,"beginning of my routine = %p", my_routine);
  if (confirm_call(addr, my_routine)) {
    return UNW_ADDR_CONFIRMED;
  }
  if (indirect_or_tail(addr, my_routine)) {
    return UNW_ADDR_PROBABLE;
  }

  return UNW_ADDR_WRONG;
}


static validation_status
simple_validate_addr(void *addr, void *generic)
{
  void *beg, *end;
  if (fnbounds_enclosing_addr(addr, &beg, &end)) {
    return UNW_ADDR_WRONG;
  }

  return UNW_ADDR_PROBABLE;
}


static int 
csprof_check_fence(void *ip)
{
  return monitor_in_start_func_wide(ip);
}



//****************************************************************************
// debug operations
//****************************************************************************

unw_cursor_t _dbg_cursor;

void dbg_init_cursor(void *context)
{
  DEBUG_NO_LONGJMP = 1;
  unw_init_cursor_arch(context, &_dbg_cursor);
  DEBUG_NO_LONGJMP = 0;
}
