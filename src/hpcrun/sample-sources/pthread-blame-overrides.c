// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "pthread-blame-overrides.h"

#include <pthread.h>

//
// Investigate TBB
//
#include <sched.h>

#include <semaphore.h>
#include <stdio.h>

#include "blame-shift/blame-map.h"
#include "pthread-blame.h"
#include "../thread_data.h"

#include "../hpctoolkit.h"
#include "../safe-sampling.h"
#include "../sample_event.h"
#include "../messages/messages.h"

//
// NOTE 1: the following functions (apparently) need dlvsym instead of dlsym to obtain the
//         true function:
//           pthread_cond_broadcast
//           pthread_cond_signal
//           pthread_cond_wait
//           pthread_cond_timedwait;
//
//  !! This treatment is determined empirically !!
//  Future systems may need different treatment (or different version for dlvsym)
//
//
// NOTE 2: Rather than use a constructor, use lazy initialization.
//         If constructor needed, uncomment code below
//
// void
// HPCRUN_CONTRUCTOR(pthread_plugin_init)()
// {
//   DL_LOOKUPV(pthread_cond_broadcast);
//   DL_LOOKUPV(pthread_cond_signal);
//   DL_LOOKUPV(pthread_cond_wait);
//   DL_LOOKUPV(pthread_cond_timedwait);
//
//   DL_LOOKUP(pthread_mutex_lock);
//   DL_LOOKUP(pthread_mutex_unlock);
//   DL_LOOKUP(pthread_mutex_timedlock);
// }
//

int
hpcrun_pthread_cond_timedwait(pthread_cond_t* restrict cond,
    pthread_mutex_t* restrict mutex, const struct timespec* restrict abstime,
    const struct hpcrun_foil_appdispatch_libc_sync* dispatch)
{
  pthread_directed_blame_shift_blocked_start(cond);
  int retval = f_pthread_cond_timedwait(cond, mutex, abstime, dispatch);
  pthread_directed_blame_shift_end();

  return retval;
}

int
hpcrun_pthread_cond_wait(pthread_cond_t* restrict cond, pthread_mutex_t* restrict mutex,
                           const struct hpcrun_foil_appdispatch_libc_sync* dispatch)
{
  pthread_directed_blame_shift_blocked_start(cond);
  int retval = f_pthread_cond_wait(cond, mutex, dispatch);
  pthread_directed_blame_shift_end();

  return retval;
}

int
hpcrun_pthread_cond_broadcast(pthread_cond_t *cond,
                                const struct hpcrun_foil_appdispatch_libc_sync* dispatch)
{
  int retval = f_pthread_cond_broadcast(cond, dispatch);
  pthread_directed_blame_accept(cond);
  return retval;
}

int
hpcrun_pthread_cond_signal(pthread_cond_t* cond,
                             const struct hpcrun_foil_appdispatch_libc_sync* dispatch)
{
  int retval = f_pthread_cond_signal(cond, dispatch);
  pthread_directed_blame_accept(cond);
  return retval;
}

int
hpcrun_pthread_mutex_lock(pthread_mutex_t* mutex,
                            const struct hpcrun_foil_appdispatch_libc_sync* dispatch)
{
  TMSG(LOCKWAIT, "mutex lock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return f_pthread_mutex_lock(mutex, dispatch);
  }

  TMSG(LOCKWAIT, "pthread mutex LOCK override");
  pthread_directed_blame_shift_blocked_start(mutex);
  int retval = f_pthread_mutex_lock(mutex, dispatch);
  pthread_directed_blame_shift_end();

  return retval;
}

int
hpcrun_pthread_mutex_unlock(pthread_mutex_t* mutex,
                              const struct hpcrun_foil_appdispatch_libc_sync* dispatch)
{
  TMSG(LOCKWAIT, "mutex unlock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return f_pthread_mutex_unlock(mutex, dispatch);
  }
  TMSG(LOCKWAIT, "pthread mutex UNLOCK");
  int retval = f_pthread_mutex_unlock(mutex, dispatch);
  pthread_directed_blame_accept(mutex);
  return retval;
}

int
hpcrun_pthread_mutex_timedlock(
    pthread_mutex_t* restrict mutex, const struct timespec* restrict abs_timeout,
    const struct hpcrun_foil_appdispatch_libc_sync* dispatch)
{
  TMSG(LOCKWAIT, "mutex timedlock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return f_pthread_mutex_timedlock(mutex, abs_timeout, dispatch);
  }

  TMSG(LOCKWAIT, "pthread mutex TIMEDLOCK");

  pthread_directed_blame_shift_blocked_start(mutex);
  int retval = f_pthread_mutex_timedlock(mutex, abs_timeout, dispatch);
  pthread_directed_blame_shift_end();
  return retval;
}

int
hpcrun_pthread_spin_lock(pthread_spinlock_t* lock,
                           const struct hpcrun_foil_appdispatch_libc_sync* dispatch)
{
  TMSG(LOCKWAIT, "pthread_spin_lock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return f_pthread_spin_lock(lock, dispatch);
  }

  TMSG(LOCKWAIT, "pthread SPIN LOCK override");
  pthread_directed_blame_shift_spin_start((void*) lock);
  int retval = f_pthread_spin_lock(lock, dispatch);
  pthread_directed_blame_shift_end();

  return retval;
}

int
hpcrun_pthread_spin_unlock(pthread_spinlock_t* lock,
                             const struct hpcrun_foil_appdispatch_libc_sync* dispatch)
{
  TMSG(LOCKWAIT, "pthread_spin_unlock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return f_pthread_spin_unlock(lock, dispatch);
  }

  TMSG(LOCKWAIT, "pthread SPIN UNLOCK");
  int retval = f_pthread_spin_unlock(lock, dispatch);
  pthread_directed_blame_accept((void*) lock);
  return retval;
}

//
// Further determination of TBB mechanisms
//

// **** global vars for now ****
//

static unsigned int calls_to_sched_yield = 0;
static unsigned int calls_to_sem_wait    = 0;

int
hpcrun_sched_yield(const struct hpcrun_foil_appdispatch_libc_sync* dispatch)
{
  // TMSG(TBB_EACH, "sched_yield hit");

  int retval = f_sched_yield(dispatch);

  // __sync_fetch_and_add(&calls_to_sched_yield, 1);
  return retval;
}

int
hpcrun_sem_wait(sem_t* sem, const struct hpcrun_foil_appdispatch_libc_sync* dispatch)
{
  if (! pthread_blame_lockwait_enabled() ) {
    return f_sem_wait(sem, dispatch);
  }
  TMSG(TBB_EACH, "sem wait hit, sem = %p", sem);
  pthread_directed_blame_shift_blocked_start(sem);
  int retval = f_sem_wait(sem, dispatch);
  pthread_directed_blame_shift_end();

  // hpcrun_atomicIncr(&calls_to_sem_wait);
  // calls_to_sem_wait++;
  return retval;
}

int
hpcrun_sem_post(sem_t* sem, const struct hpcrun_foil_appdispatch_libc_sync* dispatch)
{
  TMSG(LOCKWAIT, "sem_post ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return f_sem_post(sem, dispatch);
  }
  TMSG(LOCKWAIT, "sem POST");
  int retval = f_sem_post(sem, dispatch);
  pthread_directed_blame_accept(sem);
  // TMSG(TBB_EACH, "sem post hit, sem = %p", sem);

  return retval;
}

int
hpcrun_sem_timedwait(sem_t* sem, const struct timespec* abs_timeout,
                       const struct hpcrun_foil_appdispatch_libc_sync* dispatch)
{
  TMSG(TBB_EACH, "sem timedwait hit, sem = %p", sem);
  int retval = f_sem_timedwait(sem, abs_timeout, dispatch);

  return retval;
}

void
tbb_stats(void)
{
  AMSG("TBB stuff: ");
  AMSG("Calls to sched yield: %d", calls_to_sched_yield);
  AMSG("Calls to sem wait", calls_to_sem_wait);
}
