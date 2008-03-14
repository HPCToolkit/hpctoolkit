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
#include "find.h"
#include "pmsg.h"
#include "stack_troll.h"

unsigned int stack_troll(char **sp, unsigned int *ra_pos){
  int i;
  char **tmp_sp;

  PMSG(TROLL,"stack trolling initiated at sp = %p",sp);
  tmp_sp = sp;
  for (i = 0; i < TROLL_LIMIT; i++){
    void *s, *e;
    if (! find_enclosing_function_bounds_v(*tmp_sp,&s,&e,SILENT)){
      PMSG(TROLL,"found valid address %p at sp = %p",*tmp_sp,tmp_sp);
      *ra_pos = (unsigned long) tmp_sp - (unsigned long) sp;
      return 1;
    }
    tmp_sp++;
  }
  PMSG(TROLL,"stack troll failed");
  *ra_pos = -1;
  return 0;
}
