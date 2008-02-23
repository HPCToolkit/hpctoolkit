// NOTE: For static linked versions, this MUST BE compiled for
//       the target system, as the include file that defines the
//       structure for the contexts may be different for the
//       target system vs the build system.
// !! ESPECIALLY FOR CATAMOUNT !!
//
#include <setjmp.h>
#include <ucontext.h>

#include "state.h"
#include "dump_backtraces.h"

#include "bad_unwind.h"
#include "find.h"
#include "general.h"
#include "mem.h"
#include "intervals.h"
#include "pmsg.h"
#include "stack_troll.h"
#include "monitor.h"

#include "prim_unw_cursor.h"
#include "x86-decoder.h"

#include "splay.h"

#include "thread_data.h"

#if defined(__LIBCATAMOUNT__)
#undef __CRAYXT_CATAMOUNT_TARGET
#define __CRAYXT_CATAMOUNT_TARGET
#endif

#define NO 1
#define BUILD_INT 1

int debug_unw = 0;

/****************************************************************************************
 * private operations
 ***************************************************************************************/


static void update_cursor_with_troll(unw_cursor_t *cursor, void *sp, void *pc, void *bp);



/****************************************************************************************
 * interface functions
 ***************************************************************************************/


void
unw_init(void)
{
  extern void xed_init(void);

  
  PMSG(UNW,"UNW: xed, splay tree init");
  x86_family_decoder_init();
  csprof_interval_tree_init();
}

void unw_init_f_mcontext(mcontext_t* mctxt, unw_cursor_t *cursor)
{

  void **bp, *sp,*pc;

  PMSG(UNW,"init prim unw (mcontext) called: mctxt = %p, cursor_p = %p\n",mctxt,cursor);


#ifdef __CRAYXT_CATAMOUNT_TARGET
  pc = (void *)mctxt->sc_rip;
  bp = (void **)mctxt->sc_rbp;
  sp = (void **)mctxt->sc_rsp;
#else
  pc = (void *)mctxt->gregs[REG_RIP];
  bp = (void **)mctxt->gregs[REG_RBP];
  sp = (void **)mctxt->gregs[REG_RSP];
#endif

  PMSG(UNW,"UNW_INIT:frame pc = %p, frame bp = %p, frame sp = %p",pc,bp,sp);

  // EMSG("UNW_INIT:frame pc = %p, frame bp = %p, frame sp = %p",pc,bp,sp);

  cursor->intvl = csprof_addr_to_interval(pc);

  if (! cursor->intvl){
    PMSG(TROLL,"UNW INIT FAILURE :frame pc = %p, frame bp = %p, frame sp = %p",pc,bp,sp);
    PMSG(TROLL,"UNW INIT calls stack troll");

    update_cursor_with_troll(cursor, sp, pc, bp);
  }
  else {
    cursor->pc = pc;
    cursor->bp = bp;
    cursor->sp = sp;
  }

  if (debug_unw) {
    PMSG(UNW,"dumping the found interval");
    dump_ui(cursor->intvl,1); // debug for now
  }
  PMSG(UNW,"UNW_INIT: returned interval = %p",cursor->intvl);
}


void unw_init_f_ucontext(ucontext_t* context, unw_cursor_t *cursor)
{

  PMSG(UNW,"init prim unw called w ucontext: context = %p, cursor_p = %p\n",context,cursor);
  unw_init_f_mcontext(&(context->uc_mcontext),cursor);
}

// This get_reg just extracts the pc, regardless of REGID

int unw_get_reg(unw_cursor_t *cursor,int REGID,void **regv)
{

  *regv = cursor->pc;
  
  return 0;
}

int unw_step (unw_cursor_t *cursor)
{
  void **bp, **spr_sp, **spr_bp;
  void *sp,*pc,*spr_pc;
  unwind_interval *uw;

  // current frame
  bp = cursor->bp;
  sp = cursor->sp;
  pc = cursor->pc;
  uw = cursor->intvl;

  cursor->intvl = NULL;
  if ((cursor->intvl == NULL) && 
      (uw->ra_status == RA_SP_RELATIVE || uw->ra_status == RA_STD_FRAME)) {
    spr_sp  = ((void **)((unsigned long) sp + uw->sp_ra_pos));
    spr_pc  = *spr_sp;
    if (uw->bp_status == BP_UNCHANGED){
      spr_bp = bp;
    }
    else {
      //-----------------------------------------------------------
      // reload the candidate value for the caller's BP from the 
      // save area in the activation frame according to the unwind 
      // information produced by binary analysis
      //-----------------------------------------------------------
      spr_bp = (void **)((unsigned long) sp + uw->sp_bp_pos);
      spr_bp  = *spr_bp; 

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
      if (((unsigned long) spr_bp < (unsigned long) sp) && 
	  ((unsigned long) bp > (unsigned long) sp)) 
	spr_bp = bp;
    }
    spr_sp += 1;
    cursor->intvl = csprof_addr_to_interval(spr_pc);
  }

  if ((cursor->intvl == NULL) && 
      ((uw->ra_status == RA_BP_FRAME) || 
       ((uw->ra_status == RA_STD_FRAME) && 
	((unsigned long) bp >= (unsigned long) sp)))) {
    // bp relative
    spr_sp  = ((void **)((unsigned long) bp + uw->bp_bp_pos));
    spr_bp  = *spr_sp;
    spr_sp  = ((void **)((unsigned long) bp + uw->bp_ra_pos));
    spr_pc  = *spr_sp;
    spr_sp += 1;
    if ((unsigned long) spr_sp > (unsigned long) sp) { 
      // this condition is a weak correctness check. only
      // try building an interval for the return address again if it succeeds
      cursor->intvl = csprof_addr_to_interval(spr_pc);
    }
  }

  if (! cursor->intvl){
    if (((void *)spr_sp) >= monitor_stack_bottom()) { 
      return -1; 
    }
    
    PMSG(TROLL,"UNW STEP FAILURE :candidate pc = %p, cursor pc = %p, cursor bp = %p, cursor sp = %p",spr_pc,pc,bp,sp);
    PMSG(TROLL,"UNW STEP calls stack troll");

    update_cursor_with_troll(cursor, sp, pc, bp);
  }
  else {
    cursor->pc = spr_pc;
    cursor->bp = spr_bp;
    cursor->sp = spr_sp;
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

int _dbg_no_longjmp = 0;

static void drop_sample(void)
{
  if (_dbg_no_longjmp){
    return;
  }
  dump_backtraces(TD_GET(state),0);
  sigjmp_buf_t *it = &(TD_GET(bad_unwind));
  siglongjmp(it->jb,9);
}

static void update_cursor_with_troll(unw_cursor_t *cursor, void *sp, void *pc, void *bp)
{
  void  **spr_sp, **spr_bp, *spr_pc;

  unsigned int tmp_ra_loc;
  if (stack_troll((char **)sp,&tmp_ra_loc)){
    spr_sp  = ((void **)((unsigned long) sp + tmp_ra_loc));
    spr_pc  = *spr_sp;
#if 0
    spr_bp  = (void **) *(spr_sp - 1);
#else
    spr_bp  = (void **) bp;
#endif
    spr_sp += 1;

    cursor->intvl = csprof_addr_to_interval(spr_pc);
    if (! cursor->intvl){
      PMSG(TROLL, "No interval found for trolled pc, dropping sample,cursor pc = %p",pc);
      // assert(0);
      drop_sample();
    }
    else {
      PMSG(TROLL,"Trolling advances cursor to pc = %p,sp = %p",spr_pc,spr_sp);
      PMSG(TROLL,"TROLL SUCCESS pc = %p",pc);
      cursor->pc = spr_pc;
      cursor->bp = spr_bp;
      cursor->sp = spr_sp;
    }
  }
  else {
    PMSG(TROLL, "Troll failed: dropping sample,cursor pc = %p",pc);
    PMSG(TROLL,"TROLL FAILURE pc = %p",pc);
    // assert(0);
    drop_sample();
  }
}

/****************************************************************************************
 * debug operations
 ***************************************************************************************/

unw_cursor_t _dbg_cursor;

void dbg_init_cursor(void *context)
{
  _dbg_no_longjmp = 1;
  unw_init_f_mcontext(context,&_dbg_cursor);
  _dbg_no_longjmp = 0;
}
