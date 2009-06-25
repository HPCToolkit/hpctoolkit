/*
 * Override pthread spin lock functions.
 *
 * $Id$
 */

#include <pthread.h>
#include <stdio.h>

#include "monitor_ext.h"
#include "pmsg.h"

typedef int spinlock_init_fcn(pthread_spinlock_t *, int);
typedef int spinlock_fcn(pthread_spinlock_t *);

#ifdef HPCRUN_STATIC_LINK
extern spinlock_init_fcn __real_pthread_spin_init;
extern spinlock_fcn      __real_pthread_spin_destroy;
extern spinlock_fcn      __real_pthread_spin_lock;
extern spinlock_fcn      __real_pthread_spin_trylock;
extern spinlock_fcn      __real_pthread_spin_unlock;
#endif

static spinlock_init_fcn *real_spin_init = NULL;
static spinlock_fcn      *real_spin_destroy = NULL;
static spinlock_fcn      *real_spin_lock = NULL;
static spinlock_fcn      *real_spin_trylock = NULL;
static spinlock_fcn      *real_spin_unlock = NULL;


int
MONITOR_WRAP_NAME(pthread_spin_init)(pthread_spinlock_t *lock, int pshared)
{
  int ret;

  MONITOR_GET_NAME_WRAP(real_spin_init, pthread_spin_init);
  ret = (*real_spin_init)(lock, pshared);
  monitor_thread_spin_init(lock, ret);

  return (ret);
}

int
MONITOR_WRAP_NAME(pthread_spin_destroy)(pthread_spinlock_t *lock)
{
  int ret;
  pthread_spinlock_t *real_lock;

  MONITOR_GET_NAME_WRAP(real_spin_destroy, pthread_spin_destroy);
  real_lock = monitor_thread_pre_spin_destroy(lock);
  ret = (*real_spin_destroy)(real_lock);
  //monitor_thread_post_spin_destroy(lock, ret);

  return (ret);
}

int
MONITOR_WRAP_NAME(pthread_spin_lock)(pthread_spinlock_t *lock)
{
  int ret;
  pthread_spinlock_t *real_lock;

  MONITOR_GET_NAME_WRAP(real_spin_lock, pthread_spin_lock);
  real_lock = monitor_thread_pre_spin_lock(lock);
  ret = (*real_spin_lock)(real_lock);
  monitor_thread_post_spin_lock(lock, ret);

  return (ret);
}

int
MONITOR_WRAP_NAME(pthread_spin_trylock)(pthread_spinlock_t *lock)
{
  int ret;

  MONITOR_GET_NAME_WRAP(real_spin_trylock, pthread_spin_trylock);
  // FIXME
  ret = (*real_spin_trylock)(lock);
  monitor_thread_spin_trylock(lock, ret);

  return (ret);
}

int
MONITOR_WRAP_NAME(pthread_spin_unlock)(pthread_spinlock_t *lock)
{
  int ret;

  MONITOR_GET_NAME_WRAP(real_spin_unlock, pthread_spin_unlock);
  // FIXME
  ret = (*real_spin_unlock)(lock);
  monitor_thread_spin_unlock(lock);

  return (ret);
}
