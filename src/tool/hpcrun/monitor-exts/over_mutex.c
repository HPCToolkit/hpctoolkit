/*
 * Override pthread mutex functions.
 *
 * $Id$
 */

#include <pthread.h>
#include <stdio.h>

#include "libmonitor_upcalls.h"
#include "monitor_ext.h"
#include "pmsg.h"

typedef int mutex_lock_fcn(pthread_mutex_t *);

#ifdef HPCRUN_STATIC_LINK
extern mutex_lock_fcn __real_pthread_mutex_lock;
extern mutex_lock_fcn __real_pthread_mutex_trylock;
extern mutex_lock_fcn __real_pthread_mutex_unlock;
#endif

static mutex_lock_fcn *real_mutex_lock = NULL;
static mutex_lock_fcn *real_mutex_trylock = NULL;
static mutex_lock_fcn *real_mutex_unlock = NULL;

int
MONITOR_WRAP_NAME(pthread_mutex_lock)(pthread_mutex_t *lock)
{
  int ret;

  MONITOR_GET_NAME_WRAP(real_mutex_lock, pthread_mutex_lock);
  monitor_thread_pre_lock();
  ret = (*real_mutex_lock)(lock);
  monitor_thread_post_lock(ret);

  return ret;
}

int
MONITOR_WRAP_NAME(pthread_mutex_trylock)(pthread_mutex_t *lock)
{
  int ret;

  MONITOR_GET_NAME_WRAP(real_mutex_trylock, pthread_mutex_trylock);
  monitor_thread_pre_lock();
  ret = (*real_mutex_trylock)(lock);
  monitor_thread_post_trylock(ret);

  return ret;
}

int
MONITOR_WRAP_NAME(pthread_mutex_unlock)(pthread_mutex_t *lock)
{
  int ret;

  MONITOR_GET_NAME_WRAP(real_mutex_unlock, pthread_mutex_unlock);
  ret = (*real_mutex_unlock)(lock);
  monitor_thread_unlock();

  return ret;
}
