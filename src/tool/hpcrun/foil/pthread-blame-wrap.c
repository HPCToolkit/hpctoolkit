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
// Copyright ((c)) 2002-2023, Rice University
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

#include "foil.h"
#include "sample-sources/pthread-blame-overrides.h"

static const char symver[] = "GLIBC_2.3.2";

FOIL_WRAP_DECL(pthread_cond_timedwait)
HPCRUN_EXPOSED int
__wrap_pthread_cond_timedwait(pthread_cond_t* restrict cond,
    pthread_mutex_t* restrict mutex, const struct timespec* restrict abstime) {
  LOOKUP_FOIL_BASE(base, pthread_cond_timedwait);
  return base(&__real_pthread_cond_timedwait, cond, mutex, abstime);
}

FOIL_WRAP_DECL(pthread_cond_wait)
HPCRUN_EXPOSED int
__wrap_pthread_cond_wait(pthread_cond_t* restrict cond, pthread_mutex_t* restrict mutex) {
  LOOKUP_FOIL_BASE(base, pthread_cond_wait);
  return base(&__real_pthread_cond_wait, cond, mutex);
}

FOIL_WRAP_DECL(pthread_cond_broadcast)
HPCRUN_EXPOSED int
__wrap_pthread_cond_broadcast(pthread_cond_t *cond) {
  LOOKUP_FOIL_BASE(base, pthread_cond_broadcast);
  return base(&__real_pthread_cond_broadcast, cond);
}

FOIL_WRAP_DECL(pthread_cond_signal)
HPCRUN_EXPOSED int
__wrap_pthread_cond_signal(pthread_cond_t* cond) {
  LOOKUP_FOIL_BASE(base, pthread_cond_signal);
  return base(&__real_pthread_cond_signal, cond);
}

extern typeof(pthread_mutex_lock) __wrap_pthread_mutex_lock;
HPCRUN_EXPOSED int
__wrap_pthread_mutex_lock(pthread_mutex_t* mutex) {
  LOOKUP_FOIL_BASE(base, pthread_mutex_lock);
  return base(mutex);
}

extern typeof(pthread_mutex_unlock) __wrap_pthread_mutex_unlock;
HPCRUN_EXPOSED int
__wrap_pthread_mutex_unlock(pthread_mutex_t* mutex) {
  LOOKUP_FOIL_BASE(base, pthread_mutex_unlock);
  return base(mutex);
}

FOIL_WRAP_DECL(pthread_mutex_timedlock)
HPCRUN_EXPOSED int
__wrap_pthread_mutex_timedlock(pthread_mutex_t* restrict mutex,
    const struct timespec* restrict abs_timeout) {
  LOOKUP_FOIL_BASE(base, pthread_mutex_timedlock);
  return base(&__real_pthread_mutex_timedlock, mutex, abs_timeout);
}

FOIL_WRAP_DECL(pthread_spin_lock)
HPCRUN_EXPOSED int
__wrap_pthread_spin_lock(pthread_spinlock_t* lock) {
  LOOKUP_FOIL_BASE(base, pthread_spin_lock);
  return base(&__real_pthread_spin_lock, lock);
}

FOIL_WRAP_DECL(pthread_spin_unlock)
HPCRUN_EXPOSED int
__wrap_pthread_spin_unlock(pthread_spinlock_t* lock) {
  LOOKUP_FOIL_BASE(base, pthread_spin_unlock);
  return base(&__real_pthread_spin_unlock, lock);
}

FOIL_WRAP_DECL(sched_yield)
HPCRUN_EXPOSED int
__wrap_sched_yield() {
  LOOKUP_FOIL_BASE(base, sched_yield);
  return base(&__real_sched_yield);
}

FOIL_WRAP_DECL(sem_wait)
HPCRUN_EXPOSED int
__wrap_sem_wait(sem_t* sem) {
  LOOKUP_FOIL_BASE(base, sem_wait);
  return base(&__real_sem_wait, sem);
}

FOIL_WRAP_DECL(sem_post)
HPCRUN_EXPOSED int
__wrap_sem_post(sem_t* sem) {
  LOOKUP_FOIL_BASE(base, sem_post);
  return base(&__real_sem_post, sem);
}

FOIL_WRAP_DECL(sem_timedwait)
HPCRUN_EXPOSED int
__wrap_sem_timedwait(sem_t* sem, const struct timespec* abs_timeout) {
  LOOKUP_FOIL_BASE(base, sem_timedwait);
  return base(&__real_sem_timedwait, sem, abs_timeout);
}
