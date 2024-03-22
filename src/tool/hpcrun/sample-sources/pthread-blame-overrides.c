// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

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
foilbase_pthread_cond_timedwait(fn_pthread_cond_timedwait_t* real_fn, pthread_cond_t* restrict cond,
    pthread_mutex_t* restrict mutex, const struct timespec* restrict abstime)
{
  pthread_directed_blame_shift_blocked_start(cond);
  int retval = real_fn(cond, mutex, abstime);
  pthread_directed_blame_shift_end();

  return retval;
}

int
foilbase_pthread_cond_wait(fn_pthread_cond_wait_t* real_fn, pthread_cond_t* restrict cond,
                           pthread_mutex_t* restrict mutex)
{
  pthread_directed_blame_shift_blocked_start(cond);
  int retval = real_fn(cond, mutex);
  pthread_directed_blame_shift_end();

  return retval;
}

int
foilbase_pthread_cond_broadcast(fn_pthread_cond_broadcast_t* real_fn, pthread_cond_t *cond)
{
  int retval = real_fn(cond);
  pthread_directed_blame_accept(cond);
  return retval;
}

int
foilbase_pthread_cond_signal(fn_pthread_cond_signal_t* real_fn, pthread_cond_t* cond)
{
  int retval = real_fn(cond);
  pthread_directed_blame_accept(cond);
  return retval;
}

int
foilbase_pthread_mutex_lock(fn_pthread_mutex_lock_t* real_fn, pthread_mutex_t* mutex)
{
  TMSG(LOCKWAIT, "mutex lock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return real_fn(mutex);
  }

  TMSG(LOCKWAIT, "pthread mutex LOCK override");
  pthread_directed_blame_shift_blocked_start(mutex);
  int retval = real_fn(mutex);
  pthread_directed_blame_shift_end();

  return retval;
}

int
foilbase_pthread_mutex_unlock(fn_pthread_mutex_unlock_t* real_fn, pthread_mutex_t* mutex)
{
  TMSG(LOCKWAIT, "mutex unlock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return real_fn(mutex);
  }
  TMSG(LOCKWAIT, "pthread mutex UNLOCK");
  int retval = real_fn(mutex);
  pthread_directed_blame_accept(mutex);
  return retval;
}

int
foilbase_pthread_mutex_timedlock(fn_pthread_mutex_timedlock_t* real_fn,
    pthread_mutex_t* restrict mutex, const struct timespec* restrict abs_timeout)
{
  TMSG(LOCKWAIT, "mutex timedlock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return real_fn(mutex, abs_timeout);
  }

  TMSG(LOCKWAIT, "pthread mutex TIMEDLOCK");

  pthread_directed_blame_shift_blocked_start(mutex);
  int retval = real_fn(mutex, abs_timeout);
  pthread_directed_blame_shift_end();
  return retval;
}

int
foilbase_pthread_spin_lock(fn_pthread_spin_lock_t* real_fn, pthread_spinlock_t* lock)
{
  TMSG(LOCKWAIT, "pthread_spin_lock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return real_fn(lock);
  }

  TMSG(LOCKWAIT, "pthread SPIN LOCK override");
  pthread_directed_blame_shift_spin_start((void*) lock);
  int retval = real_fn(lock);
  pthread_directed_blame_shift_end();

  return retval;
}

int
foilbase_pthread_spin_unlock(fn_pthread_spin_unlock_t* real_fn, pthread_spinlock_t* lock)
{
  TMSG(LOCKWAIT, "pthread_spin_unlock ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return real_fn(lock);
  }

  TMSG(LOCKWAIT, "pthread SPIN UNLOCK");
  int retval = real_fn(lock);
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
foilbase_sched_yield(fn_sched_yield_t* real_fn)
{
  // TMSG(TBB_EACH, "sched_yield hit");

  int retval = real_fn();

  // __sync_fetch_and_add(&calls_to_sched_yield, 1);
  return retval;
}

int
foilbase_sem_wait(fn_sem_wait_t* real_fn, sem_t* sem)
{
  if (! pthread_blame_lockwait_enabled() ) {
    return real_fn(sem);
  }
  TMSG(TBB_EACH, "sem wait hit, sem = %p", sem);
  pthread_directed_blame_shift_blocked_start(sem);
  int retval = real_fn(sem);
  pthread_directed_blame_shift_end();

  // hpcrun_atomicIncr(&calls_to_sem_wait);
  // calls_to_sem_wait++;
  return retval;
}

int
foilbase_sem_post(fn_sem_post_t* real_fn, sem_t* sem)
{
  TMSG(LOCKWAIT, "sem_post ENCOUNTERED");
  if (! pthread_blame_lockwait_enabled() ) {
    return real_fn(sem);
  }
  TMSG(LOCKWAIT, "sem POST");
  int retval = real_fn(sem);
  pthread_directed_blame_accept(sem);
  // TMSG(TBB_EACH, "sem post hit, sem = %p", sem);

  return retval;
}

int
foilbase_sem_timedwait(fn_sem_timedwait_t* real_fn, sem_t* sem, const struct timespec* abs_timeout)
{
  TMSG(TBB_EACH, "sem timedwait hit, sem = %p", sem);
  int retval = real_fn(sem, abs_timeout);

  return retval;
}

void
tbb_stats(void)
{
  AMSG("TBB stuff: ");
  AMSG("Calls to sched yield: %d", calls_to_sched_yield);
  AMSG("Calls to sem wait", calls_to_sem_wait);
}
