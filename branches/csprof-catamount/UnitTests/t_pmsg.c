#include <stdio.h>
#include "pmsg.h"

int main(int argc, char *argv[]){
  PMSG(ALL,"ALL:hello %s f PMSG","%repl string%");
  PMSG(FIND,"FIND shows");
  PMSG(XED,"XED shows, next string nothing\n-------Nothing below----------");
  PMSG(NONE,"This should NOT show");

  PMSG(ALL,"\n--------mask set to XED------------");
  
  set_mask_f_numstr("2");
  PMSG(ALL,"ALL:hello %s f PMSG","%repl string%");
  PMSG(FIND,"FIND shows");
  PMSG(XED,"XED shows, next string nothing\n-------Nothing below----------");
  PMSG(NONE,"This should NOT show");

  return 0;
}
