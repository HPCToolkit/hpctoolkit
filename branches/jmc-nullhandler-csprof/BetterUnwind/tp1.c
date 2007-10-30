#include <stdio.h>

extern void xed_init(void);
extern void xed_inst(void *ins);
extern int xed_inst2(void *ins);

#include "intervals.h"
#include "find.h"

int tfn(int chk){
  if (chk) {
    printf("inside chk\n");
    return 0;
  } else {
    printf("inside else\n");
    return 1;
  }
  return 2;
}

int main(int argc, char *argv[]){

  void *p = (void *)&tfn;
  int len,i;

  tfn(0);
  // xed_inst(&tfn);

  xed_init();

  printf("building intervals for tfn\n");
  printf("--------------------------\n");
  
  build_intervals(&tfn, (char *) &main - (char *) &tfn);

  find_enclosing_function_bounds((char *) &raidev_process_recv);

  printf("\n\n\n");
  printf("building intervals for raidev_process_recv\n");
  printf("--------------------------\n");
  build_intervals(&raidev_process_recv, (char *) &MPID_DeviceCheck - (char *) &raidev_process_recv);
#if 0

  for (i=0; i < 10; i++){
    printf("decode instruction @addr %p\n",p);
    len = xed_inst2(p);
    xed_show();
    p += len;
  }
#endif

  return 0;
}

