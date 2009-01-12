/*
 * Troll the stack for potential valid return addresses

 * The algorithm is simple, given a stack pointer (address), look thru the
 * stack for a value that is an address in some function (and therefore is
 * a valid return address). If such a value is found, then the address (eg the
 * stack pointer) is returned.
 * 
 * Valid return address is determined by using the enclosing_function_bounds
 * routine.
 * 
 * NOTE: This is a secondary heuristic: when the normal binary analysis interval
 * builder does not yield a useful interval, the unwinder can use this stack trolling
 * method.
 */

#include <stdlib.h>

#include <include/general.h>

#include "pmsg.h"
#include "stack_troll.h"
#include "fnbounds_interface.h"

uint stack_troll(void **start_sp, uint *ra_pos)
{
  void **sp = start_sp;

  for (int i = 0; i < TROLL_LIMIT; i++) {
    void *beg, *end;
    if (!fnbounds_enclosing_addr(*sp, &beg, &end)) {
      PMSG(TROLL,"troll(sp=%p): found valid address %p at sp=%p", 
	   start_sp, *sp, sp);
      *ra_pos = (uintptr_t)sp - (uintptr_t)start_sp;
      return 1;
    }
    sp++;
  }
  
  PMSG(TROLL,"troll(sp=%p): failed using limit %d", start_sp, TROLL_LIMIT);
  *ra_pos = -1;
  return 0;
}
