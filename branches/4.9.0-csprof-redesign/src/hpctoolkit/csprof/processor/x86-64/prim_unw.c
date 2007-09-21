// NOTE: For static linked versions, this MUST BE compiled for
//       the target system, as the include file that defines the
//       structure for the contexts may be different for the
//       target system vs the build system.
// !! ESPECIALLY FOR CATAMOUNT !!
//
#include <setjmp.h>
#include <ucontext.h>

#include "bad_unwind.h"
#include "find.h"
#include "general.h"
#include "intervals.h"


#define NO 1
#define BUILD_INT 1

// #undef CSPROF_THREADS

#ifdef CSPROF_THREADS
#include <pthread.h>
#include <sys/types.h>

pthread_mutex_t xedlock = PTHREAD_MUTEX_INITIALIZER;

#endif

interval_status build_intervals(char  *ins, unsigned int len){
  interval_status rv;

#ifdef CSPROF_THREADS
  pthread_mutex_lock(&xedlock);
#endif

  rv = l_build_intervals(ins,len);

#ifdef CSPROF_THREADS
  pthread_mutex_unlock(&xedlock);
#endif
  return rv;
}

void unw_init(void){
  extern void xed_init(void);

  xed_init();
#ifdef CSPROF_THREADS
  pthread_mutex_init(&xedlock,NULL);
#endif

}

// This unwinder presumes that there are stack frames
#include "prim_unw_cursor.h"

void unw_init_f_context(void *context,unw_cursor_t *frame){

  char *start,*end;
  int bad_encl;
  interval_status istat;

  MSG(1,"init prim unw called: context = %p, cursor_p = %p\n",context,frame);
  ucontext_t *ctx = (ucontext_t *) context;

#ifdef SOME_OTHER_MCONTEXT
  frame->pc = (void *)ctx->uc_mcontext.sc_rip;
  frame->bp = (void **)ctx->uc_mcontext.sc_rbp;
#else
  frame->pc = (void *)ctx->uc_mcontext.gregs[REG_RIP];
  frame->bp = (void **)ctx->uc_mcontext.gregs[REG_RBP];
  frame->sp = (void **)ctx->uc_mcontext.gregs[REG_RSP];
#endif

  bad_encl = find_enclosing_function_bounds(frame->pc, &start, &end);
  if (bad_encl){
    MSG(1," -->NO enclosing function bounds for %p",frame->pc);
  } else {
    MSG(1,"INIT:for function @ %p, start = %p, end = %p",frame->pc,start,end);
#ifdef BUILD_INT
    istat = build_intervals(start,end-start);
#ifdef NO
    MSG(1,"INIT: interval list code=%d, list head=%p",istat.errcode,istat.first);
#endif
    frame->intvl = istat.first;
#endif // BUILD_INT
  }

  MSG(1,"frame pc = %p, frame bp = %p\n",frame->pc,frame->bp);
}

void unw_init_f_mcontext(void *context,unw_cursor_t *frame){

  char *start,*end;
  int bad_encl;
  interval_status istat;

  MSG(1,"init prim unw called: context = %p, cursor_p = %p\n",context,frame);
  mcontext_t *ctx = (mcontext_t *) context;

#ifdef SOME_OTHER_MCONTEXT
  frame->pc = (void *)ctx->sc_rip;
  frame->bp = (void **)ctx->sc_rbp;
  frame->bp = (void **)ctx->sc_rsp;
#else
  frame->pc = (void *)ctx->gregs[REG_RIP];
  frame->bp = (void **)ctx->gregs[REG_RBP];
  frame->sp = (void **)ctx->gregs[REG_RSP];
#endif

#ifdef NO
  MSG(1,"INIT:frame pc = %p, frame bp = %p, frame sp = %p",frame->pc,frame->bp,
      frame->sp);
#endif
  bad_encl = find_enclosing_function_bounds(frame->pc, &start, &end);
  if (bad_encl){
    MSG(1," -->NO enclosing function bounds for %p",frame->pc);
    frame->intvl = 0; // no intervals if no enclosing fn bounds
  } else {
#ifdef NO
    MSG(1,"INIT:for function @ %p, start = %p, end = %p",frame->pc,start,end);
#endif
    // FIXME: Check errcode before assigning interval list
#ifdef BUILD_INT
    istat = build_intervals(start,end-start);
#ifdef NO
    MSG(1,"INIT: interval list code=%d, list head=%p",istat.errcode,istat.first);
#endif
    frame->intvl = istat.first;
#endif // BUILD_INT
  }
}

// This get_reg just extracts the pc, regardless of REGID

int unw_get_reg(unw_cursor_t *cursor,int REGID,void **regv){

  *regv = cursor->pc;
  
  return 0;
}

// can probably assume that interval is found, but
// still return NULL if no found
//
unwind_interval *find_interval(void *pc, unwind_interval *ilist){
  unwind_interval *cur;
  unsigned long ipc = (unsigned long) pc;

  // MSG(1,"finding interval in ilist w pc=%p,ilist = %p",pc,ilist);
  for(cur = ilist; cur != 0; cur = cur->next){
    // MSG(1,"searching ipc=%lx,current start=%lx, current end=%lx",ipc,
    //     cur->startaddr,cur->endaddr);
    if ((cur->startaddr <= ipc) &&(ipc < cur->endaddr)){
      return cur;
    }
  }
  return 0;
}

int unw_step (unw_cursor_t *cursor){
  void **bp, **spr_sp, **spr_bp;
  void *sp,*pc,*spr_pc;
  interval_status istat;
  unwind_interval *uw;

  char *start,*end;
  int bad_encl;

  // current frame
  bp         = cursor->bp;
  sp         = cursor->sp;
  pc         = cursor->pc;

  // MSG(1,"interval list start %p",cursor->intvl);
  uw         = find_interval(pc,cursor->intvl);

  if (! uw ){

    EMSG("!!! No interval found!!!,cursor pc = %p",pc);
    _jb *it = get_bad_unwind();
    siglongjmp(it->jb,9);

  } else {
    // MSG(1,"dumping the found interval");
    // idump(uw); // debug for now
  }

  // spr rel step
  // FIXME: next bp needs to check if this is a frame procedure
   
  spr_sp     = ((void **)((unsigned long) sp + uw->ra_pos));
  spr_pc     = *spr_sp;
  spr_bp     = (void **) *(spr_sp -1);
  spr_sp    += 1; 

  // advance frame using bp
  // FIXME: factor this out into a different routine

#if 0
  pc         = bp[1];
  sp         = &(bp[1]) + 1;
  bp         = (void **) bp[0];

  MSG(1,"--STEP check--: sp = %p, spr_sp = %p -- pc = %p, spr_pc = %p,"
      " -- bp = %p, spr_bp = %p",
      sp,spr_sp,pc,spr_pc,bp,spr_bp);
#endif

  cursor->pc = spr_pc;
  cursor->bp = spr_bp;
  cursor->sp = spr_sp;

  bad_encl = find_enclosing_function_bounds(spr_pc, &start, &end);
  if (bad_encl){
    MSG(1," -->STEP:NO enclosing function bounds for %p",spr_pc);
  } else {
#ifdef NO
    MSG(1,"STEP:for function @ %p, start = %p, end = %p",spr_pc,start,end);
    MSG(1,"STEP, build intervals for %p",spr_pc);
#endif
    istat = build_intervals(start,end-start);
#ifdef NO
    MSG(1,"STEP: interval list code=%d, list head=%p",istat.errcode,istat.first);
#endif
    cursor->intvl = istat.first;
  }

#ifdef NO
  MSG(1,"NEXT frame pc = %p, frame bp = %p\n",cursor->pc,cursor->bp);
#endif

  return 1;
}
