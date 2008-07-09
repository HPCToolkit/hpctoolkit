#include "context.h"
#include "monitor.h"
#include "pmsg.h"
#include "splay.h"
#include "unwind.h"
#include "unwind_cursor.h"


#if 0
#  define RA_OFFSET_FROM_BP 2
#endif
#define RA_OFFSET_FROM_BP 1
// #define NEXT_PC_FROM_BP(bp) ((void *) *(((long *) bp) + RA_OFFSET_FROM_BP))
#define NEXT_PC_FROM_BP(bp) (*((void **) bp + RA_OFFSET_FROM_BP))


void
unw_init_arch(void)
{
}


int 
unw_step(unw_cursor_t *cursor)
{
  void *next_pc, **next_sp, **next_bp;

  // current frame
  void **bp  = cursor->bp;
  void **sp  = cursor->sp;
  void *pc   = cursor->pc;

  //-----------------------------------------------------------
  // if we fail this check, we are done with the unwind.
  //-----------------------------------------------------------
  if (monitor_in_start_func_wide(pc)) {
    TMSG(UNWIND,"monitor in start func wide, pc=%p",pc);
    return 0;
  }
  
  //-----------------------------------------------------------
  //  BP relative unwind
  //-----------------------------------------------------------
  next_pc  = NEXT_PC_FROM_BP(bp);
  next_bp  = *bp;                                    // follow back chain
  next_sp  = *bp;

  TMSG(UNWIND,"candidate next_pc = %p, candidate next_bp = %p",next_pc,next_bp);
  if ((unsigned long) next_sp < (unsigned long) sp){
    TMSG(UNWIND,"next sp = %p < current sp = %p",next_sp,sp);
    return 0;
  }

  cursor->intvl = csprof_addr_to_interval(next_pc);

  if (!cursor->intvl) {
    TMSG(UNWIND,"next_pc = %p has no valid interval, continuing to look ...",next_pc);
    if (((void *)next_sp) >= monitor_stack_bottom()){
      TMSG(UNWIND,"next sp (%p) >= monitor_stack_bottom (%p)",next_sp,monitor_stack_bottom());
      return 0;
    }
    // there isn't a valid PC saved in the LR slot in this frame.
    // try one frame deeper. 
    //
    // NOTE: this should only happen in the top frame on the stack.
    next_pc = NEXT_PC_FROM_BP(next_bp);
    TMSG(UNWIND,"skipping up one frame f candidate bp = %p ==> next pc = %p",next_bp,next_pc);
    cursor->intvl = csprof_addr_to_interval(next_pc);
  }

  if (!cursor->intvl){
    TMSG(UNWIND,"candidate next_pc = %p did not have an interval",next_pc);
    return 0;
  }

  cursor->pc = next_pc;
  cursor->bp = next_bp;
  cursor->sp = next_sp;

  TMSG(UNWIND,"step goes forward w pc = %p, bp = %p",next_pc,next_bp);
  return 1;
}
