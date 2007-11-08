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

#include "prim_unw_cursor.h"

#include "splay.h"

#if defined(__LIBCATAMOUNT__)
#undef __CRAYXT_CATAMOUNT_TARGET
#define __CRAYXT_CATAMOUNT_TARGET
#endif

#define NO 1
#define BUILD_INT 1

int debug_unw = 0;

void unw_init(void){
  extern void xed_init(void);

  
  PMSG(UNW,"UNW: xed, splay tree init");
  xed_init();
  csprof_interval_tree_init();
}

void unw_init_f_mcontext(void *context,unw_cursor_t *cursor){

  void **bp, **spr_sp, **spr_bp;
  void *sp,*pc,*spr_pc;

  PMSG(UNW,"init prim unw (mcontext) called: context = %p, cursor_p = %p\n",context,cursor);
  mcontext_t *ctx = (mcontext_t *) context;


#ifdef __CRAYXT_CATAMOUNT_TARGET
  pc = (void *)ctx->sc_rip;
  bp = (void **)ctx->sc_rbp;
  sp = (void **)ctx->sc_rsp;
#else
  pc = (void *)ctx->gregs[REG_RIP];
  bp = (void **)ctx->gregs[REG_RBP];
  sp = (void **)ctx->gregs[REG_RSP];
#endif

  PMSG(UNW,"UNW_INIT:frame pc = %p, frame bp = %p, frame sp = %p",pc,bp,sp);

  // EMSG("UNW_INIT:frame pc = %p, frame bp = %p, frame sp = %p",pc,bp,sp);

  cursor->intvl = csprof_addr_to_interval((unsigned long)pc);

  if (! cursor->intvl){
    PMSG(TROLL,"UNW INIT FAILURE :frame pc = %p, frame bp = %p, frame sp = %p",pc,bp,sp);
    PMSG(TROLL,"UNW INIT calls stack troll");

    unsigned int tmp_ra_loc;
    if (stack_troll((char **)sp,&tmp_ra_loc)){
      spr_sp  = ((void **)((unsigned long) sp + tmp_ra_loc));
      spr_pc  = *spr_sp;
      spr_bp  = (void **) *(spr_sp - 1);
      spr_sp += 1;

      cursor->intvl = csprof_addr_to_interval((unsigned long)spr_pc);
      if (! cursor->intvl){
	EMSG("!!! No interval found!!!,cursor pc = %p",pc);
	// assert(0);
	dump_backtraces(csprof_get_state(),0);
	_jb *it = get_bad_unwind();
	siglongjmp(it->jb,9);
      }
      else {
	cursor->pc = spr_pc;
	cursor->bp = spr_bp;
	cursor->sp = spr_sp;
      }
    }
  }
  else {
    cursor->pc = pc;
    cursor->bp = bp;
    cursor->sp = sp;
  }
  if (debug_unw) {
    PMSG(UNW,"dumping the found interval");
    idump(cursor->intvl); // debug for now
  }
  PMSG(UNW,"UNW_INIT: returned interval = %p",cursor->intvl);
}

void unw_init_f_ucontext(void *context,unw_cursor_t *cursor){

  PMSG(UNW,"init prim unw called w ucontext: context = %p, cursor_p = %p\n",context,cursor);
  ucontext_t *ctx = (ucontext_t *) context;

  unw_init_f_mcontext((void *) &(ctx->uc_mcontext),cursor);
}

// This get_reg just extracts the pc, regardless of REGID

int unw_get_reg(unw_cursor_t *cursor,int REGID,void **regv){

  *regv = cursor->pc;
  
  return 0;
}

int unw_step (unw_cursor_t *cursor){
  void **bp, **spr_sp, **spr_bp;
  void *sp,*pc,*spr_pc;
  unwind_interval *uw;

  // current frame
  bp = cursor->bp;
  sp = cursor->sp;
  pc = cursor->pc;
  uw = cursor->intvl;

  // spr rel step
  // FIXME: next bp needs to check if this is a frame procedure
   
  spr_sp  = ((void **)((unsigned long) sp + uw->ra_pos));
  spr_pc  = *spr_sp;
  spr_bp  = (void **) *(spr_sp -1);
  spr_sp += 1; 

  cursor->intvl = csprof_addr_to_interval((unsigned long)spr_pc);

  if (! cursor->intvl){
    PMSG(TROLL,"UNW STEP FAILURE :candidate pc = %p, cursor pc = %p, cursor bp = %p, cursor sp = %p",spr_pc,pc,bp,sp);
    PMSG(TROLL,"UNW STEP calls stack troll");

    unsigned int tmp_ra_loc;
    if (stack_troll((char **)sp,&tmp_ra_loc)){
      spr_sp  = ((void **)((unsigned long) sp + tmp_ra_loc));
      spr_pc  = *spr_sp;
      spr_bp  = (void **) *(spr_sp - 1);
      spr_sp += 1;

      cursor->intvl = csprof_addr_to_interval((unsigned long)spr_pc);
      if (! cursor->intvl){
	EMSG("!!! No interval found!!!,cursor pc = %p",pc);
	// assert(0);
	dump_backtraces(csprof_get_state(),0);
	_jb *it = get_bad_unwind();
	siglongjmp(it->jb,9);
      }
      else {
	PMSG(TROLL,"Trolling advances cursor to pc = %p,sp = %p",spr_pc,spr_sp);
	cursor->pc = spr_pc;
	cursor->bp = spr_bp;
	cursor->sp = spr_sp;
      }
    }
  }
  else {
    cursor->pc = spr_pc;
    cursor->bp = spr_bp;
    cursor->sp = spr_sp;
  }
  if (debug_unw) {
    PMSG(UNW,"dumping the found interval");
    idump(cursor->intvl); // debug for now
  }

  PMSG(UNW,"NEXT frame pc = %p, frame bp = %p\n",cursor->pc,cursor->bp);

  return 1;
}
