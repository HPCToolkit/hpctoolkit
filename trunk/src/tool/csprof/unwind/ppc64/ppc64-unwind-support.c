#include "context.h"
#include "monitor.h"
#include "pmsg.h"
#include "splay.h"
#include "unwind.h"
#include "unwind_cursor.h"


#define RA_OFFSET_FROM_BP 2
#define NEXT_PC_FROM_BP(bp) ((void *) *(((long *) bp) + RA_OFFSET_FROM_BP))


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
  if (monitor_in_start_func_wide(pc)) return 0;
  
  //-----------------------------------------------------------
  //  BP relative unwind
  //-----------------------------------------------------------
  next_pc  = NEXT_PC_FROM_BP(bp);
  next_bp  = *bp;                                    // follow back chain
  next_sp  = *bp;

  if ((unsigned long) next_sp < (unsigned long) sp) return 0; 

  cursor->intvl = csprof_addr_to_interval(next_pc);

  if (!cursor->intvl) {
    if (((void *)next_sp) >= monitor_stack_bottom()) return 0; 

    // there isn't a valid PC saved in the LR slot in this frame.
    // try one frame deeper. 
    //
    // NOTE: this should only happen in the top frame on the stack.
    next_pc = NEXT_PC_FROM_BP(next_bp);
    cursor->intvl = csprof_addr_to_interval(next_pc);
  }

  if (!cursor->intvl) return 0;

  cursor->pc = next_pc;
  cursor->bp = next_bp;
  cursor->sp = next_sp;

#if 0
  if (debug_unw) {
    PMSG(UNW,"dumping the found interval");
    dump_ui(cursor->intvl,1); // debug for now
  }

  PMSG(UNW,"NEXT frame pc = %p, frame bp = %p\n", cursor->pc, cursor->bp);
#endif

  return 1;
}
