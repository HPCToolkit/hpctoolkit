/* driver implementation using interval timers and SIGPROF */

/* system includes */
#include <stddef.h>
#include <sys/time.h>           /* setitimer() */
#include <ucontext.h>           /* struct ucontext */

#include "csprof_options.h"
// #include "interface.h"
#include "pmsg.h"
#include "sample_event.h"

#include "monitor.h"

#ifndef STATIC_ONLY
#define CSPROF_PROFILE_SIGNAL SIGPROF
#define CSPROF_PROFILE_TIMER ITIMER_PROF
#else
#define CSPROF_PROFILE_SIGNAL SIGALRM
#define CSPROF_PROFILE_TIMER ITIMER_REAL
#endif

static struct itimerval itimer;

unsigned long sample_period = 0L;
unsigned long seconds = 0L;
unsigned long microseconds = 0L;

void
csprof_init_timer(csprof_options_t *options)
{
    sample_period = options->sample_period;
    seconds = sample_period / 1000000;
    microseconds = sample_period % 1000000;

    PMSG(ITIMER_HANDLER,"init timer w sample_period = %ld, seconds = %ld, usec = %ld",
        sample_period, seconds, microseconds);
    /* begin timing after the given delay */
    itimer.it_value.tv_sec = seconds;
    itimer.it_value.tv_usec = microseconds;

    itimer.it_interval.tv_sec = 0;
    itimer.it_interval.tv_usec = 0;
}

void
csprof_set_timer(void){

  setitimer(CSPROF_PROFILE_TIMER, &itimer, NULL);
  PMSG(ITIMER_HANDLER,"called csprof_set_timer");

}

void
csprof_disable_timer(void){
  struct itimerval itimer;
  timerclear(&itimer.it_value);
  setitimer(CSPROF_PROFILE_TIMER, &itimer, NULL);
}

static int
csprof_itimer_signal_handler(int sig, siginfo_t *siginfo, void *context){

  PMSG(ITIMER_HANDLER,"Itimer sample event");
  csprof_sample_event(context);
  csprof_set_timer();

  return 0; /* tell monitor that the signal has been handled */
}

void
csprof_init_itimer_signal_handler(void){
  monitor_sigaction(CSPROF_PROFILE_SIGNAL, &csprof_itimer_signal_handler, 0, NULL);
}

void
itimer_event_init(csprof_options_t *opts){
  // csprof_init_signal_handler();
  csprof_init_timer(opts);
  csprof_set_timer();
}

