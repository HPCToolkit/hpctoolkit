#ifndef MEM_ERROR_GEN_H
#define MEM_ERROR_GEN_H

#include "mem_error_dbg.h"

#if GEN_INF_MEM_REQ
  // special purpose extreme error condition checking code
  extern int samples_taken;
  if (samples_taken >= 4){
    TMSG(SPECIAL,"Hit infinite interval build to test mem kill");
    int msg_prt = 0;
    for(int i = 1;;i++){
      unwind_interval *u = (unwind_interval *) csprof_malloc(sizeof(unwind_interval));
      if (! u && ! msg_prt){
        TMSG(SPECIAL,"after %d intervals, memory failure",i);
        msg_prt = 1;
      }
    }
  }
#endif // GEN_INF_MEM_REQ

#endif //MEM_ERROR_GEN_H
