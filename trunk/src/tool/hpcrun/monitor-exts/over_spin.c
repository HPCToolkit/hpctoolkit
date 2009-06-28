// -*-Mode: C++;-*- // technically C99
// $Id$

/*
 * Override pthread spin lock functions.
 *
 */

#include <pthread.h>
#include <stdio.h>

#include "monitor_ext.h"
#include "pmsg.h"

typedef int spinlock_fcn(pthread_spinlock_t *);

#ifdef HPCRUN_STATIC_LINK
extern spinlock_fcn __real_pthread_spin_lock;
extern spinlock_fcn __real_pthread_spin_trylock;
extern spinlock_fcn __real_pthread_spin_unlock;
#endif

static spinlock_fcn *real_spin_lock = NULL;
static spinlock_fcn *real_spin_trylock = NULL;
static spinlock_fcn *real_spin_unlock = NULL;


int
MONITOR_WRAP_NAME(pthread_spin_lock)(pthread_spinlock_t *lock)
{
  int ret;

  MONITOR_GET_NAME_WRAP(real_spin_lock, pthread_spin_lock);
  monitor_thread_pre_spin_lock(lock);
  ret = (*real_spin_lock)(lock);
  monitor_thread_post_spin_lock(lock, ret);

  return (ret);
}

int
MONITOR_WRAP_NAME(pthread_spin_trylock)(pthread_spinlock_t *lock)
{
  int ret;

  MONITOR_GET_NAME_WRAP(real_spin_trylock, pthread_spin_trylock);
  //monitor_thread_pre_spin_trylock(lock); // tallent: needed?
  ret = (*real_spin_trylock)(lock);
  monitor_thread_post_spin_trylock(lock, ret);

  return (ret);
}

int
MONITOR_WRAP_NAME(pthread_spin_unlock)(pthread_spinlock_t *lock)
{
  int ret;

  MONITOR_GET_NAME_WRAP(real_spin_unlock, pthread_spin_unlock);
  //monitor_thread_pre_spin_unlock(lock); // tallent: needed?
  ret = (*real_spin_unlock)(lock);
  monitor_thread_post_spin_unlock(lock, ret);

  return (ret);
}
