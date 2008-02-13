/* driver implementation using interval timers and SIGPROF */

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stddef.h>
#include <sys/time.h>           /* setitimer() */
#include <ucontext.h>           /* struct ucontext */



/******************************************************************************
 * local includes
 *****************************************************************************/

/*---------------------------
 * monitor
 *--------------------------*/
#include "monitor.h"

/*---------------------------
 * csprof
 *--------------------------*/
#include "csprof_options.h"
#include "pmsg.h"
#include "sample_event.h"
#include "metrics_types.h"

/******************************************************************************
 * macros
 *****************************************************************************/

// FIXME: this should not be keyed on STATIC_ONLY
#ifndef STATIC_ONLY
#define CSPROF_PROFILE_SIGNAL SIGPROF
#define CSPROF_PROFILE_TIMER ITIMER_PROF
#else
#define CSPROF_PROFILE_SIGNAL SIGALRM
#define CSPROF_PROFILE_TIMER ITIMER_REAL
#endif


/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static int
csprof_itimer_signal_handler(int sig, siginfo_t *siginfo, ucontext_t* context);



/******************************************************************************
 * local variables
 *****************************************************************************/

static struct itimerval itimer;

static unsigned long sample_period = 0L;
static unsigned long seconds = 0L;
static unsigned long microseconds = 0L;



/******************************************************************************
 * interface operations
 *****************************************************************************/

void
csprof_init_timer(csprof_options_t *options)
{
  sample_period = options->sample_period;
  seconds = sample_period / 1000000;
  microseconds = sample_period % 1000000;

  PMSG(ITIMER_HANDLER,"init timer w sample_period = %ld, seconds = %ld, usec = %ld",
       sample_period, seconds, microseconds);

  /* signal once after the given delay */
  itimer.it_value.tv_sec = seconds;
  itimer.it_value.tv_usec = microseconds;

  itimer.it_interval.tv_sec = 0;
  itimer.it_interval.tv_usec = 0;
}


int
csprof_itimer_start(void)
{
  return setitimer(CSPROF_PROFILE_TIMER, &itimer, NULL);
}


int
csprof_itimer_stop(void)
{
  struct itimerval itimer;
  int rc;

  timerclear(&itimer.it_value);
  timerclear(&itimer.it_interval);
  rc = setitimer(CSPROF_PROFILE_TIMER, &itimer, NULL);
  return rc;
}


void
csprof_itimer_init(csprof_options_t *opts)
{
  csprof_init_timer(opts);
  monitor_sigaction(CSPROF_PROFILE_SIGNAL, 
		    (monitor_sighandler_t*)&csprof_itimer_signal_handler,
		    0, NULL);
}



/******************************************************************************
 * private operations 
 *****************************************************************************/

static int
csprof_itimer_signal_handler(int sig, siginfo_t *siginfo, ucontext_t* context)
{
  PMSG(ITIMER_HANDLER,"Itimer sample event");
  csprof_sample_event(context, WEIGHT_METRIC, 1);
  csprof_itimer_start();

  return 0; /* tell monitor that the signal has been handled */
}
