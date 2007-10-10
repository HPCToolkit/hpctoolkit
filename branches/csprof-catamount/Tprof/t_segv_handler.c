#include <sys/types.h>
#include <stddef.h>
#include <signal.h>
#include <dlfcn.h>
#include <setjmp.h>

#include "general.h"

extern int real_sigaction(int sig, const struct sigaction *act,struct sigaction *oact);

static struct sigaction previous_sigsegv_handler;
static int catching_sigsegv = 0;
int segv_count = 0;

static void
csprof_sigsegv_signal_handler(int sig, siginfo_t *siginfo, void *context)
{
    segv_count++;

    if(!catching_sigsegv) {
        MSG(1,"Received SIGSEGV at %s,%d", __FILE__, __LINE__);
    }
}

int
setup_segv(void){

  struct sigaction sa;
  int ret;

  /* set up handler for catching sigsegv */
  sa.sa_sigaction = csprof_sigsegv_signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;

  return 0;

  ret = sigaction(SIGSEGV, &sa, &previous_sigsegv_handler);

  if(ret != 0) {
    ERRMSG("Unable to install SIGSEGV handler", __FILE__, __LINE__);
  }
  return ret;
}
