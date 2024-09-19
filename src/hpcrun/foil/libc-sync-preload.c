// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "common-preload.h"
#include "common.h"
#include "libc-private.h"

#include <assert.h>
#include <malloc.h>
#include <stdarg.h>
#include <threads.h>

static struct hpcrun_foil_appdispatch_libc_sync dispatch_val;

static void init_dispatch() {
  dispatch_val = (struct hpcrun_foil_appdispatch_libc_sync){
      .pthread_cond_timedwait = foil_dlvsym("pthread_cond_timedwait", "GLIBC_2.3.2"),
      .pthread_cond_wait = foil_dlvsym("pthread_cond_wait", "GLIBC_2.3.2"),
      .pthread_cond_broadcast = foil_dlvsym("pthread_cond_broadcast", "GLIBC_2.3.2"),
      .pthread_cond_signal = foil_dlvsym("pthread_cond_signal", "GLIBC_2.3.2"),
      .pthread_mutex_lock = foil_dlsym("pthread_mutex_lock"),
      .pthread_mutex_unlock = foil_dlsym("pthread_mutex_unlock"),
      .pthread_mutex_timedlock = foil_dlsym("pthread_mutex_timedlock"),
      .pthread_spin_lock = foil_dlsym("pthread_spin_lock"),
      .pthread_spin_unlock = foil_dlsym("pthread_spin_unlock"),
      .sched_yield = foil_dlsym("sched_yield"),
      .sem_wait = foil_dlsym("sem_wait"),
      .sem_post = foil_dlsym("sem_post"),
      .sem_timedwait = foil_dlsym("sem_timedwait"),
  };
}

static const struct hpcrun_foil_appdispatch_libc_sync* dispatch() {
  static once_flag once = ONCE_FLAG_INIT;
  call_once(&once, init_dispatch);
  return &dispatch_val;
}

HPCRUN_EXPOSED_API int pthread_cond_timedwait(pthread_cond_t* restrict cond,
                                              pthread_mutex_t* restrict mutex,
                                              const struct timespec* restrict abstime) {
  return hpcrun_foil_fetch_hooks_libc_dl()->pthread_cond_timedwait(cond, mutex, abstime,
                                                                   dispatch());
}

HPCRUN_EXPOSED_API int pthread_cond_wait(pthread_cond_t* restrict cond,
                                         pthread_mutex_t* restrict mutex) {
  return hpcrun_foil_fetch_hooks_libc_dl()->pthread_cond_wait(cond, mutex, dispatch());
}

HPCRUN_EXPOSED_API int pthread_cond_broadcast(pthread_cond_t* cond) {
  return hpcrun_foil_fetch_hooks_libc_dl()->pthread_cond_broadcast(cond, dispatch());
}

HPCRUN_EXPOSED_API int pthread_cond_signal(pthread_cond_t* cond) {
  return hpcrun_foil_fetch_hooks_libc_dl()->pthread_cond_signal(cond, dispatch());
}

HPCRUN_EXPOSED_API int pthread_mutex_lock(pthread_mutex_t* mutex) {
  return hpcrun_foil_fetch_hooks_libc_dl()->pthread_mutex_lock(mutex, dispatch());
}

HPCRUN_EXPOSED_API int pthread_mutex_unlock(pthread_mutex_t* mutex) {
  return hpcrun_foil_fetch_hooks_libc_dl()->pthread_mutex_unlock(mutex, dispatch());
}

HPCRUN_EXPOSED_API int
pthread_mutex_timedlock(pthread_mutex_t* restrict mutex,
                        const struct timespec* restrict abs_timeout) {
  return hpcrun_foil_fetch_hooks_libc_dl()->pthread_mutex_timedlock(mutex, abs_timeout,
                                                                    dispatch());
}

HPCRUN_EXPOSED_API int pthread_spin_lock(pthread_spinlock_t* lock) {
  return hpcrun_foil_fetch_hooks_libc_dl()->pthread_spin_lock(lock, dispatch());
}

HPCRUN_EXPOSED_API int pthread_spin_unlock(pthread_spinlock_t* lock) {
  return hpcrun_foil_fetch_hooks_libc_dl()->pthread_spin_unlock(lock, dispatch());
}

HPCRUN_EXPOSED_API int sched_yield() {
  return hpcrun_foil_fetch_hooks_libc_dl()->sched_yield(dispatch());
}

HPCRUN_EXPOSED_API int sem_wait(sem_t* sem) {
  return hpcrun_foil_fetch_hooks_libc_dl()->sem_wait(sem, dispatch());
}

HPCRUN_EXPOSED_API int sem_post(sem_t* sem) {
  return hpcrun_foil_fetch_hooks_libc_dl()->sem_post(sem, dispatch());
}

HPCRUN_EXPOSED_API int sem_timedwait(sem_t* sem, const struct timespec* abs_timeout) {
  return hpcrun_foil_fetch_hooks_libc_dl()->sem_timedwait(sem, abs_timeout, dispatch());
}
