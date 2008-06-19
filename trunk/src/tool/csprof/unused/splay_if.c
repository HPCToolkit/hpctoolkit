/* *****
   This file is a test of the splay tree interface, but implementing it
    using the "always build" strategy
   ***** */
#include "find.h"
#include "general.h"
#include "intervals.h"

#ifdef CSPROF_THREADS
#include <pthread.h>
#include <sys/types.h>

pthread_mutex_t xedlock = PTHREAD_MUTEX_INITIALIZER;

#endif

static interval_status build_intervals(char *ins, unsigned int len){
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

// can probably assume that interval is found, but
// still return NULL if no found
//
unwind_interval *find_interval(unsigned long ipc, unwind_interval *ilist){
  unwind_interval *cur;
  // unsigned long ipc = (unsigned long) pc;

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

#define NO 1
unwind_interval *csprof_addr_to_interval(unsigned long addr){
  interval_status istat;
  char *start,*end;
  int bad_encl;

  bad_encl = find_enclosing_function_bounds((void *)addr, &start, &end);
  if (bad_encl){
    MSG(1," -->STEP:NO enclosing function bounds for %p",addr);
    return 0;
  } else {
#ifdef NO
    MSG(1,"STEP:for function @ %p, start = %p, end = %p",addr,start,end);
    MSG(1,"STEP, build intervals for %p",addr);
#endif
    istat = build_intervals(start,end-start);
#ifdef NO
    MSG(1,"STEP: interval list code=%d, list head=%p",istat.errcode,istat.first);
#endif
    return find_interval(addr,istat.first);
  }
}

void csprof_interval_tree_init(void){
#ifdef CSPROF_THREADS
  pthread_mutex_init(&xedlock,NULL);
#endif
}

