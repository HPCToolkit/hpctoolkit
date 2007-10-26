#include <stdio.h>

extern void xed_init(void);
extern void xed_inst(void *ins);
extern int xed_inst2(void *ins);

#include "intervals.h"
#include "find.h"
#include "pmsg.h"

#include "foo.c"

int main(int argc, char *argv[])
{
  xed_init();
  set_pmsg_mask(ALL);

  printf("building intervals for chk\n");
  printf("--------------------------\n");
  
  l_build_intervals(chk, sizeof(chk));

  return 0;
}

