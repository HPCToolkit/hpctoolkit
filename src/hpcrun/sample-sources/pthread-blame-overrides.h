// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef HPCRUN_SS_PTHREAD_BLAME_OVERRIDES_H
#define HPCRUN_SS_PTHREAD_BLAME_OVERRIDES_H

#include <pthread.h>
#include <semaphore.h>

typedef int fn_pthread_cond_timedwait_t(pthread_cond_t* restrict cond,
    pthread_mutex_t* restrict mutex, const struct timespec* restrict abstime);
extern fn_pthread_cond_timedwait_t pthread_cond_timedwait;
int
foilbase_pthread_cond_timedwait(fn_pthread_cond_timedwait_t* real_fn, pthread_cond_t* restrict cond,
    pthread_mutex_t* restrict mutex, const struct timespec* restrict abstime);

typedef int fn_pthread_cond_wait_t(pthread_cond_t* restrict cond,
                                   pthread_mutex_t* restrict mutex);
extern fn_pthread_cond_wait_t pthread_cond_wait;
int
foilbase_pthread_cond_wait(fn_pthread_cond_wait_t* real_fn, pthread_cond_t* restrict cond,
                           pthread_mutex_t* restrict mutex);

typedef int fn_pthread_cond_broadcast_t(pthread_cond_t* cond);
extern fn_pthread_cond_broadcast_t pthread_cond_broadcast;
int
foilbase_pthread_cond_broadcast(fn_pthread_cond_broadcast_t* real_fn, pthread_cond_t *cond);

typedef int fn_pthread_cond_signal_t(pthread_cond_t* cond);
extern fn_pthread_cond_signal_t pthread_cond_signal;
int
foilbase_pthread_cond_signal(fn_pthread_cond_signal_t* real_fn, pthread_cond_t* cond);

typedef int fn_pthread_mutex_lock_t(pthread_mutex_t* mutex);
extern fn_pthread_mutex_lock_t pthread_mutex_lock;
int
foilbase_pthread_mutex_lock(fn_pthread_mutex_lock_t* real_fn, pthread_mutex_t* mutex);

typedef int fn_pthread_mutex_unlock_t(pthread_mutex_t* mutex);
extern fn_pthread_mutex_unlock_t pthread_mutex_unlock;
int
foilbase_pthread_mutex_unlock(fn_pthread_mutex_unlock_t* real_fn, pthread_mutex_t* mutex);

typedef int fn_pthread_mutex_timedlock_t(pthread_mutex_t* restrict mutex,
                                         const struct timespec* restrict abs_timeout);
extern fn_pthread_mutex_timedlock_t pthread_mutex_timedlock;
int
foilbase_pthread_mutex_timedlock(fn_pthread_mutex_timedlock_t* real_fn,
    pthread_mutex_t* restrict mutex, const struct timespec* restrict abs_timeout);

typedef int fn_pthread_spin_lock_t(pthread_spinlock_t* lock);
extern fn_pthread_spin_lock_t pthread_spin_lock;
int
foilbase_pthread_spin_lock(fn_pthread_spin_lock_t* real_fn, pthread_spinlock_t* lock);

typedef int fn_pthread_spin_unlock_t(pthread_spinlock_t* lock);
extern fn_pthread_spin_unlock_t pthread_spin_unlock;
int
foilbase_pthread_spin_unlock(fn_pthread_spin_unlock_t* real_fn, pthread_spinlock_t* lock);

typedef int fn_sched_yield_t(void);
extern fn_sched_yield_t sched_yield;
int
foilbase_sched_yield(fn_sched_yield_t* real_fn);

typedef int fn_sem_wait_t(sem_t* sem);
extern fn_sem_wait_t sem_wait;
int
foilbase_sem_wait(fn_sem_wait_t* real_fn, sem_t* sem);

typedef int fn_sem_post_t(sem_t* sem);
extern fn_sem_post_t sem_post;
int
foilbase_sem_post(fn_sem_post_t* real_fn, sem_t* sem);

typedef int fn_sem_timedwait_t(sem_t* sem, const struct timespec* abs_timeout);
extern fn_sem_timedwait_t sem_timedwait;
int
foilbase_sem_timedwait(fn_sem_timedwait_t* real_fn, sem_t* sem, const struct timespec* abs_timeout);

#endif  // HPCRUN_SS_PTHREAD_BLAME_OVERRIDES_H
