/*
 * Override pthread cond wait functions.
 *
 * $Id$
 */

#include <pthread.h>
#include <stdio.h>
#include <time.h>

#include "libmonitor_upcalls.h"
#include "monitor_ext.h"
#include "pmsg.h"

typedef int cond_init_fcn(pthread_cond_t *, const pthread_condattr_t *);
typedef int cond_destroy_fcn(pthread_cond_t *);
typedef int cond_wait_fcn(pthread_cond_t *, pthread_mutex_t *);
typedef int cond_timedwait_fcn(pthread_cond_t *, pthread_mutex_t *,
			       const struct timespec *);
typedef int cond_signal_fcn(pthread_cond_t *);

#ifdef HPCRUN_STATIC_LINK
extern cond_init_fcn    __real_pthread_cond_init;
extern cond_destroy_fcn __real_pthread_cond_destroy;
extern cond_wait_fcn      __real_pthread_cond_wait;
extern cond_timedwait_fcn __real_pthread_cond_timedwait;
extern cond_signal_fcn __real_pthread_cond_signal;
extern cond_signal_fcn __real_pthread_cond_broadcast;
#endif

static cond_init_fcn    *real_cond_init = NULL;
static cond_destroy_fcn *real_cond_destroy = NULL;
static cond_wait_fcn      *real_cond_wait = NULL;
static cond_timedwait_fcn *real_cond_timedwait = NULL;
static cond_signal_fcn *real_cond_signal = NULL;
static cond_signal_fcn *real_cond_broadcast = NULL;

/*
 * Note: glibc defines multiple revisions of the cond-wait functions
 * (ugh).  So, we need to override all of the cond-wait functions so
 * that we get a consistent set.
 */
int
MONITOR_WRAP_NAME(pthread_cond_init)(pthread_cond_t *cond,
				     const pthread_condattr_t *attr)
{
  MONITOR_GET_NAME_WRAP(real_cond_init, pthread_cond_init);
  return (*real_cond_init)(cond, attr);
}

int
MONITOR_WRAP_NAME(pthread_cond_destroy)(pthread_cond_t *cond)
{
  MONITOR_GET_NAME_WRAP(real_cond_destroy, pthread_cond_destroy);
  return (*real_cond_destroy)(cond);
}

int
MONITOR_WRAP_NAME(pthread_cond_wait)(pthread_cond_t *cond,
				     pthread_mutex_t *mutex)
{
  int ret;

  MONITOR_GET_NAME_WRAP(real_cond_wait, pthread_cond_wait);
  monitor_thread_pre_cond_wait();
  ret = (*real_cond_wait)(cond, mutex);
  monitor_thread_post_cond_wait(ret);

  return ret;
}

int
MONITOR_WRAP_NAME(pthread_cond_timedwait)(pthread_cond_t *cond,
					  pthread_mutex_t *mutex,
					  const struct timespec *tspec)
{
  int ret;

  MONITOR_GET_NAME_WRAP(real_cond_timedwait, pthread_cond_timedwait);
  monitor_thread_pre_cond_wait();
  ret = (*real_cond_timedwait)(cond, mutex, tspec);
  monitor_thread_post_cond_wait(ret);

  return ret;
}

int
MONITOR_WRAP_NAME(pthread_cond_signal)(pthread_cond_t *cond)
{
  MONITOR_GET_NAME_WRAP(real_cond_signal, pthread_cond_signal);
  return (*real_cond_signal)(cond);
}

int
MONITOR_WRAP_NAME(pthread_cond_broadcast)(pthread_cond_t *cond)
{
  MONITOR_GET_NAME_WRAP(real_cond_broadcast, pthread_cond_broadcast);
  return (*real_cond_broadcast)(cond);
}
