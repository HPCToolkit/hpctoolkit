#include <signal.h>
#include <sys/signal.h>         /* sigaction() */
#include <sys/time.h>           /* setitimer() */

#include "msg.h"

static struct itimerval itimer;

static 

static void set_timer(void){
  setitimer(ITIMER_PROF, &itimer,NULL);
}

static void itimer_signal_handler(int sig, siginfo_t *siginfo, void *context){
  MSG("Got itimer signal!!");
  set_timer();
}

void init_signal_handler(void){
  struct sigaction sa;
  int ret;

  MSG("Got to init signal handler");

  sa.sa_sigaction = itimer_signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  ret = sigaction(SIGPROF, &sa, 0);
  return ret;
}

void hpc_init_(void){
  // extern void csprof_init_internal(void);

  //  csprof_init_internal();
  init_timer_val();
  setup_timer_signal();
  set_timer();
}
