// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef HPCRUN_SS_PTHREAD_BLAME_OVERRIDES_H
#define HPCRUN_SS_PTHREAD_BLAME_OVERRIDES_H

#include <pthread.h>
#include <semaphore.h>

#include "../foil/libc.h"

int
hpcrun_pthread_cond_timedwait(pthread_cond_t* restrict cond,
    pthread_mutex_t* restrict mutex, const struct timespec* restrict abstime,
    const struct hpcrun_foil_appdispatch_libc_sync* dispatch);

int
hpcrun_pthread_cond_wait(pthread_cond_t* restrict cond,
                           pthread_mutex_t* restrict mutex,
                           const struct hpcrun_foil_appdispatch_libc_sync* dispatch);

int
hpcrun_pthread_cond_broadcast(
    pthread_cond_t *cond, const struct hpcrun_foil_appdispatch_libc_sync* dispatch);

int
hpcrun_pthread_cond_signal(
    pthread_cond_t* cond, const struct hpcrun_foil_appdispatch_libc_sync* dispatch);

int
hpcrun_pthread_mutex_lock(
    pthread_mutex_t* mutex, const struct hpcrun_foil_appdispatch_libc_sync* dispatch);

int
hpcrun_pthread_mutex_unlock(
    pthread_mutex_t* mutex, const struct hpcrun_foil_appdispatch_libc_sync* dispatch);

int
hpcrun_pthread_mutex_timedlock(
    pthread_mutex_t* restrict mutex, const struct timespec* restrict abs_timeout,
    const struct hpcrun_foil_appdispatch_libc_sync* dispatch);

int
hpcrun_pthread_spin_lock(
    pthread_spinlock_t* lock, const struct hpcrun_foil_appdispatch_libc_sync* dispatch);

int
hpcrun_pthread_spin_unlock(
    pthread_spinlock_t* lock, const struct hpcrun_foil_appdispatch_libc_sync* dispatch);

int
hpcrun_sched_yield(const struct hpcrun_foil_appdispatch_libc_sync* dispatch);

int
hpcrun_sem_wait(sem_t* sem, const struct hpcrun_foil_appdispatch_libc_sync* dispatch);

int
hpcrun_sem_post(sem_t* sem, const struct hpcrun_foil_appdispatch_libc_sync* dispatch);

int
hpcrun_sem_timedwait(sem_t* sem, const struct timespec* abs_timeout,
                       const struct hpcrun_foil_appdispatch_libc_sync* dispatch);

#endif  // HPCRUN_SS_PTHREAD_BLAME_OVERRIDES_H
