// static int debug = 0;

extern "C" {
extern int nm_bound(unsigned long pc, unsigned long *st, unsigned long *e);
#include "pmsg.h"

int find_enclosing_function_bounds(char *addr, char **start, char **end);
}

int find_enclosing_function_bounds(char *addr, char **start, char **end){
  // debug = 1;

  int failure = 0;
  int nmb = nm_bound((unsigned long)addr,(unsigned long *)start,(unsigned long *)end);

  if (nmb){
    PMSG(FIND,"FIND:found in nm table for %p: start=%p,end=%p\n",addr, *start, *end);
  }
  else {
    EMSG("FIND:look up failed @ %p!\n",addr); // FIXME: change to EMSG??
    failure = 1;
  }
  return failure;
}

