#include <sys/types.h>
#include <stddef.h>
#include <ucontext.h>
#include <signal.h>
#include <dlfcn.h>
#include <setjmp.h>

#include "bad_unwind.h"
// #include "general.h"
#include "pmsg.h"

#include "thread_data.h"
/* catch SIGSEGVs */

int segv_count = 0;
extern int csprof_sample;

void csprof_sigsegv_signal_handler(int sig, siginfo_t *siginfo, void *context){

  if (csprof_sample){
    _jb *it = get_bad_unwind();

    segv_count++;

    /* make sure we don't hose ourselves by doing re-entrant backtracing */
    siglongjmp(it->jb,9);
  }
  else {
    EMSG("NON-SAMPLING SEGV!");
  }
}

static struct sigaction previous_sigsegv_handler;

int setup_segv(void){
  struct sigaction sa;
  int ret;

  /* set up handler for catching sigsegv */
  sa.sa_sigaction = csprof_sigsegv_signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;

  ret = sigaction(SIGSEGV, &sa, &previous_sigsegv_handler);

  if(ret != 0) {
    EMSG("Unable to install SIGSEGV handler", __FILE__, __LINE__);
  }
  return ret;
}
