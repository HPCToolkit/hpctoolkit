// note: 
//     when cross compiling, this version MUST BE compiled for
//     the target system, as the include file that defines the
//     structure for the contexts may be different for the
//     target system vs the build system.

#include <stdio.h>
#include <setjmp.h>
#include <stdbool.h>

// for getpid
#include <sys/types.h>
#include <unistd.h>

#include "state.h"
#include "dump_backtraces.h"

#include "general.h"
#include "mem.h"
#include "pmsg.h"
#include "stack_troll.h"
#include "monitor.h"

#include "unwind.h"
#include "splay.h"
#include "ui_tree.h"

#include "thread_data.h"
#include "x86-unwind-interval.h"
#include "validate_return_addr.h"

/****************************************************************************************
 * global data 
 ***************************************************************************************/

int debug_unw = 0;



/****************************************************************************************
 * local data 
 ***************************************************************************************/

static int DEBUG_WAIT_BEFORE_TROLLING = 1; // flag value won't matter environment var set

static int DEBUG_NO_LONGJMP = 0;



/****************************************************************************************
 * forward declarations 
 ***************************************************************************************/

static void update_cursor_with_troll(unw_cursor_t *cursor, int offset);

static int csprof_check_fence(void *ip);

static int unw_step_prefer_sp(void);

static void drop_sample(void);

static validation_status validate_return_addr(void *addr, void *generic_arg);

/****************************************************************************************
 * interface functions
 ***************************************************************************************/


void
unw_init(void)
{
  unw_init_arch();
  csprof_interval_tree_init();
}


void 
unw_init_cursor(void* context, unw_cursor_t *cursor)
{
  extern void dump_ui(unwind_interval *u, int dump_to_stdout);

  //  TMSG(UNW,"init prim unw called with context = %p, cursor_p = %p\n",context, cursor);

  unw_init_cursor_arch(context, cursor);

  TMSG(UNW,"INIT CURSOR: frame pc = %p, frame bp = %p, frame sp = %p", 
       cursor->pc, cursor->bp, cursor->sp);

  cursor->trolling_used = 0;
  cursor->intvl = csprof_addr_to_interval(cursor->pc);

  if (!cursor->intvl) {
    PMSG(TROLL,"UNW INIT FAILURE: frame pc = %p, frame bp = %p, frame sp = %p",
         cursor->pc, cursor->bp, cursor->sp);

    PMSG(TROLL,"UNW INIT calls stack troll");

    update_cursor_with_troll(cursor, 0);
  } 

  TMSG(UNW,"dumping the found interval");
  dump_ui((unwind_interval *)cursor->intvl, 0); // debug for now
}


// This get_reg just extracts the pc, regardless of REGID

int 
unw_get_reg(unw_cursor_t *cursor, int reg_id,void **reg_value)
{
   unw_get_reg_arch(cursor, reg_id, reg_value);
   return 0;
}

// FIXME: make this a selectable paramter, so that all manner of strategies can be selected
static int
unw_step_prefer_sp(void)
{
  if (ENABLED(PREFER_SP)){
    return 1;
  }
  else {
    return 0;
  }
  // return cursor->trolling_used;
}

step_state
unw_step_sp(unw_cursor_t *cursor)
{
  void *sp, **bp, *pc; 
  void **next_sp, **next_bp, *next_pc;

  unwind_interval *uw;

  TMSG(UNW_STRATEGY,"Using SP step");

  // current frame
  bp = cursor->bp;
  sp = cursor->sp;
  pc = cursor->pc;
  uw = (unwind_interval *)cursor->intvl;
  TMSG(UNW,"cursor in ==> bp=%p,sp=%p,pc=%p",bp,sp,pc);
  TMSG(UNW,"unwind interval in below:");
  dump_ui(uw, 0);

  next_sp  = ((void **)((unsigned long) sp + uw->sp_ra_pos));
  next_pc  = *next_sp;
  TMSG(UNW,"sp potential advance cursor: next_sp=%p ==> next_pc = %p",next_sp,next_pc);
  if (uw->bp_status == BP_UNCHANGED){
    next_bp = bp;
    TMSG(UNW,"BP_UNCHANGED ==> next_bp=%p",next_bp);
  }
  else {
    //-----------------------------------------------------------
    // reload the candidate value for the caller's BP from the 
    // save area in the activation frame according to the unwind 
    // information produced by binary analysis
    //-----------------------------------------------------------
    next_bp = (void **)((unsigned long) sp + uw->sp_bp_pos);
    TMSG(UNW,"sp unwind next_bp loc = %p",next_bp);
    next_bp  = *next_bp; 
    TMSG(UNW,"sp unwind next_bp val = %p",next_bp);

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
      TMSG(UNW,"sp unwind bp sanity check fails. resetting next_bp to current bp = %p",next_bp);
    }
  }
  next_sp += 1;
  cursor->intvl = csprof_addr_to_interval(((char *)next_pc) - 1);

  if (! cursor->intvl){
    if (((void *)next_sp) >= monitor_stack_bottom()){
      TMSG(UNW,"No next interval, and next_sp >= stack bottom, so stop unwind ...");
      return STEP_STOP;
    }
    else {
      TMSG(UNW,"No next interval, step fails");
      return STEP_ERROR;
    }
  }
  else {
    // sanity check to avoid infinite unwind loop
    if (next_sp <= cursor->sp){
      TMSG(INTV_ERR,"@ pc = %p. sp unwind does not advance stack. New sp = %p, old sp = %p",cursor->pc,next_sp,cursor->sp);
      return STEP_ERROR;
    }
    cursor->pc = next_pc;
    cursor->bp = next_bp;
    cursor->sp = next_sp;
  }

#if 0
  TMSG(UNW,"dumping the found interval");
  dump_ui((unwind_interval *)cursor->intvl,1); // debug for now
  TMSG(UNW,"NEXT frame pc = %p, frame bp = %p\n",cursor->pc,cursor->bp);
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

  TMSG(UNW,"cursor in ==> bp=%p,sp=%p,pc=%p",bp,sp,pc);
  TMSG(UNW,"unwind interval in below:");
  dump_ui(uw, 0);

  if ((unsigned long) bp >= (unsigned long) sp) {
    // bp relative
    next_sp  = ((void **)((unsigned long) bp + uw->bp_bp_pos));
    next_bp  = *next_sp;
    next_sp  = ((void **)((unsigned long) bp + uw->bp_ra_pos));
    next_pc  = *next_sp;
    next_sp += 1;
    if ((unsigned long) next_sp > (unsigned long) sp) { 
      // this condition is a weak correctness check. only
      // try building an interval for the return address again if it succeeds
      uw = (unwind_interval *)csprof_addr_to_interval(((char *)next_pc) - 1);
      if (! uw){
        if (((void *)next_sp) >= monitor_stack_bottom()) {
          TMSG(UNW_STRATEGY,"BP advance reaches monitor_stack_bottom, next_sp = %p",next_sp);
          return STEP_STOP;
        }
        TMSG(UNW_STRATEGY,"BP cannot build interval for next_pc(%p)",next_pc);
        return STEP_ERROR;
      }
      else {
        cursor->pc    = next_pc;
        cursor->bp    = next_bp;
        cursor->sp    = next_sp;
        cursor->intvl = (splay_interval_t *)uw;
        TMSG(UNW,"cursor advances ==>has_intvl=%d,bp=%p,sp=%p,pc=%p",cursor->intvl != NULL,next_bp,next_sp,next_pc);
        return STEP_OK;
      }
    }
    else {
      TMSG(UNW_STRATEGY,"BP unwind fails: next_sp(%p) <= current sp(%p)",next_sp,sp);
      return STEP_ERROR;
    }

  }
  else {
    TMSG(UNW_STRATEGY,"BP unwind fails: bp (%lx) < sp (%lx)",bp,sp);
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
  // return STEP_ERROR;
  drop_sample();
}

static step_state
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
    TMSG(UNW,"current pc in monitor fence",cursor->pc);
    return STEP_STOP;
  }

  void *sp, **bp, *pc; 
  void **next_sp, **next_bp, *next_pc;

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
  PMSG_LIMIT(TMSG(TROLL,"UNW STEP FAILURE : cursor pc = %p, cursor bp = %p, cursor sp = %p", pc, bp, sp));
  PMSG_LIMIT(TMSG(TROLL,"UNW STEP calls stack troll"));

  if (ENABLED(TROLL_WAIT)) {
    fprintf(stderr,"Hit troll point: attach w gdb to %d\n"
            "Maybe call dbg_set_flag(DBG_TROLL_WAIT,0) after attached\n",getpid());
    
    while(DEBUG_WAIT_BEFORE_TROLLING);  // spin wait for developer to attach a debugger and clear the flag 
  }
  
  update_cursor_with_troll(cursor, 1);
  return STEP_TROLL;
}

// public interface to local drop sample
void
csprof_unwind_drop_sample(void)
{
  drop_sample();
}

/****************************************************************************************
 * private operations
 ***************************************************************************************/

static void 
drop_sample(void)
{
  if (DEBUG_NO_LONGJMP) return;

  if (csprof_below_pmsg_threshold())
    dump_backtraces(TD_GET(state),0);

  csprof_up_pmsg_count();

  sigjmp_buf_t *it = &(TD_GET(bad_unwind));
  siglongjmp(it->jb,9);
}


// TODO: build up appropriate generic argument

static void 
update_cursor_with_troll(unw_cursor_t *cursor, int offset)
{
  unsigned int tmp_ra_offset;

  int ret = stack_troll(cursor->sp, &tmp_ra_offset, &validate_return_addr, (void *)cursor);
  if (ret != TROLL_INVALID) {
    void  **next_sp = ((void **)((unsigned long) cursor->sp + tmp_ra_offset));
    void *next_pc = *next_sp;

    // the current base pointer is a good assumption for the caller's base pointer 
    void **next_bp = (void **) cursor->bp; 

    next_sp += 1;
    if ( next_sp <= cursor->sp){
      PMSG_LIMIT(PMSG(TROLL,"Something wierd happened! trolling from %p resulted in sp not advancing",cursor->pc));
      drop_sample();
    }

    cursor->intvl = csprof_addr_to_interval(((char *)next_pc) + offset);
    if (cursor->intvl) {
      PMSG_LIMIT(PMSG(TROLL,"Trolling advances cursor to pc = %p,sp = %p", next_pc, next_sp));
      PMSG_LIMIT(PMSG(TROLL,"TROLL SUCCESS pc = %p", cursor->pc));

      cursor->pc = next_pc;
      cursor->bp = next_bp;
      cursor->sp = next_sp;

      cursor->trolling_used = 1;
      return; // success!
    }
    PMSG_LIMIT(PMSG(TROLL, "No interval found for trolled pc, dropping sample,cursor pc = %p", cursor->pc));
    // fall through for error handling
  }
  else {
    PMSG_LIMIT(PMSG(TROLL, "Troll failed: dropping sample,cursor pc = %p", cursor->pc));
    PMSG_LIMIT(PMSG(TROLL,"TROLL FAILURE pc = %p", cursor->pc));
    // fall through for error handling
  }
  // assert(0);
  drop_sample();
}

// XED_ICLASS_CALL_FAR XED_ICLASS_CALL_NEAR:

#include "validate_return_addr.h"
#include "fnbounds_interface.h"

static validation_status
validate_return_addr(void *addr, void *generic)
{
  void *beg, *end;
  if (fnbounds_enclosing_addr(addr, &beg, &end)){
    return UNW_ADDR_WRONG;
  }
  return UNW_ADDR_CONFIRMED;
}

static int 
csprof_check_fence(void *ip)
{
  return monitor_in_start_func_wide(ip);
}



/****************************************************************************************
 * debug operations
 ***************************************************************************************/

unw_cursor_t _dbg_cursor;

void dbg_init_cursor(void *context)
{
  DEBUG_NO_LONGJMP = 1;
  unw_init_cursor_arch(context, &_dbg_cursor);
  DEBUG_NO_LONGJMP = 0;
}
