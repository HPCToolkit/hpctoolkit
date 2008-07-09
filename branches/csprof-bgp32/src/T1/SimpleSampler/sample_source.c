#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/time.h>           /* setitimer() */
#include <ucontext.h>           /* struct ucontext */

#include "monitor.h"
#include "sample_source.h"
#include "sample_handler.h"

static int
itimer_signal_handler(int sig, siginfo_t *siginfo, void *context)
{
  process_sample(context);
  start_sample_source();
}

static struct itimerval itimer;

static int seconds = 0;
static int microseconds = 5000;

void
init_sample_source(void)
{
  itimer.it_value.tv_sec = seconds;
  itimer.it_value.tv_usec = microseconds;

  itimer.it_interval.tv_sec = seconds;
  itimer.it_interval.tv_usec = microseconds;

  monitor_sigaction(SIGPROF, &itimer_signal_handler, 0, NULL);
}

void
start_sample_source(void)
{
  int rv = setitimer(ITIMER_PROF, &itimer, NULL);
}

void
stop_sample_source(void)
{
  struct itimerval itimer;

  timerclear(&itimer.it_value);
  timerclear(&itimer.it_interval);
  int rc = setitimer(ITIMER_PROF, &itimer, NULL);
}

void
shutdown_sample_source(void)
{
  ;
}
