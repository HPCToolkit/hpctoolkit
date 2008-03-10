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
#include "handling_sample.h"

/* catch SIGSEGVs */

int segv_count = 0;
extern int csprof_sample;
extern int filtered_samples;

int
csprof_sigsegv_signal_handler(int sig, siginfo_t *siginfo, void *context)
{
  if (csprof_is_handling_sample()){
    sigjmp_buf_t *it = &(TD_GET(bad_unwind));

    segv_count++;
    siglongjmp(it->jb,9);  // make sure we don't hose ourselves by doing re-entrant backtracing
  }
  return 1; /* pass the segv to the application handler */
}


int
setup_segv(void)
{
  int ret = monitor_sigaction(SIGSEGV, &csprof_sigsegv_signal_handler, 0, NULL);

  if(ret != 0) {
    EMSG("Unable to install SIGSEGV handler", __FILE__, __LINE__);
  }
  return ret;
}
