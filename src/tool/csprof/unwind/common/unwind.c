// note: 
//     when cross compiling, this version MUST BE compiled for
//     the target system, as the include file that defines the
//     structure for the contexts may be different for the
//     target system vs the build system.

#include <stdio.h>
#include <setjmp.h>

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

#include "thread_data.h"

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

static void update_cursor_with_troll(unw_cursor_t *cursor);

static int csprof_check_fence(void *ip);


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

  PMSG(UNW,"init prim unw called with context = %p, cursor_p = %p\n",context, cursor);

  unw_init_cursor_arch(context, cursor);

  PMSG(UNW,"UNW_INIT: frame pc = %p, frame bp = %p, frame sp = %p", 
       cursor->pc, cursor->bp, cursor->sp);

  cursor->intvl = csprof_addr_to_interval(cursor->pc);

  if (!cursor->intvl) {
    PMSG(TROLL,"UNW INIT FAILURE: frame pc = %p, frame bp = %p, frame sp = %p",
         cursor->pc, cursor->bp, cursor->sp);

    PMSG(TROLL,"UNW INIT calls stack troll");

    update_cursor_with_troll(cursor);
  } 

  if (debug_unw) {
    PMSG(UNW,"dumping the found interval");
    dump_ui(cursor->intvl,1); // debug for now
  }
  PMSG(UNW,"UNW_INIT: returned interval = %p",cursor->intvl);
}


// This get_reg just extracts the pc, regardless of REGID

int 
unw_get_reg(unw_cursor_t *cursor, int reg_id,void **reg_value)
{
   unw_get_reg_arch(cursor, reg_id, reg_value);
   return 0;
}


int 
unw_step (unw_cursor_t *cursor)
{
  void *sp, **bp, *pc; 
  void **next_sp, **next_bp, *next_pc;

  unwind_interval *uw;

  // current frame
  bp = cursor->bp;
  sp = cursor->sp;
  pc = cursor->pc;
  uw = cursor->intvl;

  //-----------------------------------------------------------
  // check if we have reached the end of our unwind, which is
  // demarcated with a fence. 
  //-----------------------------------------------------------
  if (csprof_check_fence(pc)) return 0;
  
  //-----------------------------------------------------------
  // 
  //-----------------------------------------------------------
  cursor->intvl = NULL;

  if ((cursor->intvl == NULL) && 
      (uw->ra_status == RA_SP_RELATIVE || uw->ra_status == RA_STD_FRAME)) {
    next_sp  = ((void **)((unsigned long) sp + uw->sp_ra_pos));
    next_pc  = *next_sp;
    if (uw->bp_status == BP_UNCHANGED){
      next_bp = bp;
    } else {
      //-----------------------------------------------------------
      // reload the candidate value for the caller's BP from the 
      // save area in the activation frame according to the unwind 
      // information produced by binary analysis
      //-----------------------------------------------------------
      next_bp = (void **)((unsigned long) sp + uw->sp_bp_pos);
      next_bp  = *next_bp; 

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
	  ((unsigned long) bp > (unsigned long) sp)) 
	next_bp = bp;
    }
    next_sp += 1;
    cursor->intvl = csprof_addr_to_interval(next_pc);
  }

  if ((cursor->intvl == NULL) && 
      ((uw->ra_status == RA_BP_FRAME) || 
       ((uw->ra_status == RA_STD_FRAME) && 
	((unsigned long) bp >= (unsigned long) sp)))) {
    // bp relative
    next_sp  = ((void **)((unsigned long) bp + uw->bp_bp_pos));
    next_bp  = *next_sp;
    next_sp  = ((void **)((unsigned long) bp + uw->bp_ra_pos));
    next_pc  = *next_sp;
    next_sp += 1;
    if ((unsigned long) next_sp > (unsigned long) sp) { 
      // this condition is a weak correctness check. only
      // try building an interval for the return address again if it succeeds
      cursor->intvl = csprof_addr_to_interval(next_pc);
    }
  }

  if (! cursor->intvl){
    if (((void *)next_sp) >= monitor_stack_bottom()) return 0; 
    
    PMSG(TROLL,"UNW STEP FAILURE :candidate pc = %p, cursor pc = %p, cursor bp = %p, cursor sp = %p", next_pc, pc, bp, sp);
    PMSG(TROLL,"UNW STEP calls stack troll");

    IF_ENABLED(TROLL_WAIT) {
      fprintf(stderr,"Hit troll point: attach w gdb to %d\n"
	             "Maybe call dbg_set_flag(DBG_TROLL_WAIT,0) after attached\n",getpid());

      while(DEBUG_WAIT_BEFORE_TROLLING);  // spin wait for developer to attach a debugger and clear the flag 
    }

    update_cursor_with_troll(cursor);
  } else {
    cursor->pc = next_pc;
    cursor->bp = next_bp;
    cursor->sp = next_sp;
  }

  if (debug_unw) {
    PMSG(UNW,"dumping the found interval");
    dump_ui(cursor->intvl,1); // debug for now
  }

  PMSG(UNW,"NEXT frame pc = %p, frame bp = %p\n",cursor->pc,cursor->bp);

  return 1;
}



/****************************************************************************************
 * private operations
 ***************************************************************************************/

static void 
drop_sample(void)
{
  if (DEBUG_NO_LONGJMP) return;

  dump_backtraces(TD_GET(state),0);
  sigjmp_buf_t *it = &(TD_GET(bad_unwind));
  siglongjmp(it->jb,9);
}


static void 
update_cursor_with_troll(unw_cursor_t *cursor)
{
  unsigned int tmp_ra_offset;

  if (stack_troll((char **)cursor->sp, &tmp_ra_offset)) {
    void  **next_sp = ((void **)((unsigned long) cursor->sp + tmp_ra_offset));
    void *next_pc = *next_sp;

    // the current base pointer is a good assumption for the caller's base pointer 
    void **next_bp = (void **) cursor->bp; 

    next_sp += 1;

    cursor->intvl = csprof_addr_to_interval(next_pc);
    if (cursor->intvl) {
      PMSG(TROLL,"Trolling advances cursor to pc = %p,sp = %p", next_pc, next_sp);
      PMSG(TROLL,"TROLL SUCCESS pc = %p", cursor->pc);

      cursor->pc = next_pc;
      cursor->bp = next_bp;
      cursor->sp = next_sp;

      return; // success!
    }
    PMSG(TROLL, "No interval found for trolled pc, dropping sample,cursor pc = %p", cursor->pc);
    // fall through for error handling
  } else {
    PMSG(TROLL, "Troll failed: dropping sample,cursor pc = %p", cursor->pc);
    PMSG(TROLL,"TROLL FAILURE pc = %p", cursor->pc);
    // fall through for error handling
  }

  // assert(0);
  drop_sample();
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
