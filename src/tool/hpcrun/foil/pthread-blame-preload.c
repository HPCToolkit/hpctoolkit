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

HPCRUN_EXPOSED int
pthread_cond_timedwait(pthread_cond_t* restrict cond,
    pthread_mutex_t* restrict mutex, const struct timespec* restrict abstime) {
  LOOKUP_FOIL_BASE(base, pthread_cond_timedwait);
  FOIL_DLVSYM(real, pthread_cond_timedwait, symver);
  return base(real, cond, mutex, abstime);
}

HPCRUN_EXPOSED int
pthread_cond_wait(pthread_cond_t* restrict cond, pthread_mutex_t* restrict mutex) {
  LOOKUP_FOIL_BASE(base, pthread_cond_wait);
  FOIL_DLVSYM(real, pthread_cond_wait, symver);
  return base(real, cond, mutex);
}

HPCRUN_EXPOSED int
pthread_cond_broadcast(pthread_cond_t *cond) {
  LOOKUP_FOIL_BASE(base, pthread_cond_broadcast);
  FOIL_DLVSYM(real, pthread_cond_broadcast, symver);
  return base(real, cond);
}

HPCRUN_EXPOSED int
pthread_cond_signal(pthread_cond_t* cond) {
  LOOKUP_FOIL_BASE(base, pthread_cond_signal);
  FOIL_DLVSYM(real, pthread_cond_signal, symver);
  return base(real, cond);
}

HPCRUN_EXPOSED int
pthread_mutex_lock(pthread_mutex_t* mutex) {
  LOOKUP_FOIL_BASE(base, pthread_mutex_lock);
  return base(mutex);
}

HPCRUN_EXPOSED int
pthread_mutex_unlock(pthread_mutex_t* mutex) {
  LOOKUP_FOIL_BASE(base, pthread_mutex_unlock);
  return base(mutex);
}

HPCRUN_EXPOSED int
pthread_mutex_timedlock(pthread_mutex_t* restrict mutex,
    const struct timespec* restrict abs_timeout) {
  LOOKUP_FOIL_BASE(base, pthread_mutex_timedlock);
  FOIL_DLSYM(real, pthread_mutex_timedlock);
  return base(real, mutex, abs_timeout);
}

HPCRUN_EXPOSED int
pthread_spin_lock(pthread_spinlock_t* lock) {
  LOOKUP_FOIL_BASE(base, pthread_spin_lock);
  FOIL_DLSYM(real, pthread_spin_lock);
  return base(real, lock);
}

HPCRUN_EXPOSED int
pthread_spin_unlock(pthread_spinlock_t* lock) {
  LOOKUP_FOIL_BASE(base, pthread_spin_unlock);
  FOIL_DLSYM(real, pthread_spin_unlock);
  return base(real, lock);
}

HPCRUN_EXPOSED int
sched_yield() {
  LOOKUP_FOIL_BASE(base, sched_yield);
  FOIL_DLSYM(real, sched_yield);
  return base(real);
}

HPCRUN_EXPOSED int
sem_wait(sem_t* sem) {
  LOOKUP_FOIL_BASE(base, sem_wait);
  FOIL_DLSYM(real, sem_wait);
  return base(real, sem);
}

HPCRUN_EXPOSED int
sem_post(sem_t* sem) {
  LOOKUP_FOIL_BASE(base, sem_post);
  FOIL_DLSYM(real, sem_post);
  return base(real, sem);
}

HPCRUN_EXPOSED int
sem_timedwait(sem_t* sem, const struct timespec* abs_timeout) {
  LOOKUP_FOIL_BASE(base, sem_timedwait);
  FOIL_DLSYM(real, sem_timedwait);
  return base(real, sem, abs_timeout);
}
