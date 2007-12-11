#include <sys/types.h>
#include <stddef.h>
#include <ucontext.h>
#include <signal.h>
#include <dlfcn.h>
#include <setjmp.h>
#include <signal.h>

#include "bad_unwind.h"
#include "pmsg.h"
#include "monitor.h"

#include "thread_data.h"
/* catch SIGSEGVs */

int segv_count = 0;
extern int csprof_sample;

int csprof_sigsegv_signal_handler(int sig, siginfo_t *siginfo, void *context){

  if (csprof_sample){
    sigjmp_buf_t *it = get_bad_unwind();

    segv_count++;

    /* make sure we don't hose ourselves by doing re-entrant backtracing */
    siglongjmp(it->jb,9);
  }
  else {
    raise(SIGABRT);
    EMSG("NON-SAMPLING SEGV!");
  }

  return 1; /* pass the segv to the application handler */
}

static struct sigaction previous_sigsegv_handler;

int setup_segv(void){
  int ret;
#if 0
  struct sigaction sa;

  /* set up handler for catching sigsegv */
  sa.sa_sigaction = csprof_sigsegv_signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;

  ret = sigaction(SIGSEGV, &sa, &previous_sigsegv_handler);
#endif
  ret = monitor_sigaction(SIGSEGV, &csprof_sigsegv_signal_handler, 0, NULL);

  if(ret != 0) {
    EMSG("Unable to install SIGSEGV handler", __FILE__, __LINE__);
  }
  return ret;
}
