// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_LIBC_H
#define HPCRUN_FOIL_LIBC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

struct hpcrun_foil_appdispatch_libc;

int f_uulibc_start_main(
#ifdef HOST_CPU_PPC
    int argc, char** argv, char** envp, void* auxp, void (*rtld_fini)(void),
    void** stinfo, void* stack_end,
#else
    int (*main)(int, char**, char**), int argc, char** argv, void (*init)(void),
    void (*fini)(void), void (*rtld_fini)(void), void* stack_end,
#endif
    const struct hpcrun_foil_appdispatch_libc*);
void f_exit(int, const struct hpcrun_foil_appdispatch_libc*);
void f_uexit(int, const struct hpcrun_foil_appdispatch_libc*);
pid_t f_fork(const struct hpcrun_foil_appdispatch_libc*);
int f_execv(const char*, char* const[], const struct hpcrun_foil_appdispatch_libc*);
int f_execvp(const char*, char* const[], const struct hpcrun_foil_appdispatch_libc*);
int f_execve(const char*, char* const[], char* const[],
             const struct hpcrun_foil_appdispatch_libc*);
int f_sigaction(int, const struct sigaction*, struct sigaction*,
                const struct hpcrun_foil_appdispatch_libc*);
int f_sigprocmask(int, const sigset_t*, sigset_t*,
                  const struct hpcrun_foil_appdispatch_libc*);
int f_system(const char*, const struct hpcrun_foil_appdispatch_libc*);
int f_pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*,
                     const struct hpcrun_foil_appdispatch_libc*);
int f_pthread_attr_init(pthread_attr_t*, const struct hpcrun_foil_appdispatch_libc*);
int f_pthread_attr_destroy(pthread_attr_t*, const struct hpcrun_foil_appdispatch_libc*);
int f_pthread_attr_getstacksize(pthread_attr_t*, size_t*,
                                const struct hpcrun_foil_appdispatch_libc*);
int f_pthread_attr_setstacksize(pthread_attr_t*, size_t,
                                const struct hpcrun_foil_appdispatch_libc*);
void f_pthread_exit(void*, const struct hpcrun_foil_appdispatch_libc*);
int f_pthread_sigmask(int, const sigset_t*, sigset_t*,
                      const struct hpcrun_foil_appdispatch_libc*);
int f_sigwaitinfo(const sigset_t*, siginfo_t*,
                  const struct hpcrun_foil_appdispatch_libc*);
int f_sigtimedwait(const sigset_t*, siginfo_t*, const struct timespec*,
                   const struct hpcrun_foil_appdispatch_libc*);

struct hpcrun_foil_appdispatch_libc_sync;

int f_pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                             const struct timespec* abstime,
                             const struct hpcrun_foil_appdispatch_libc_sync* dispatch);
int f_pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                        const struct hpcrun_foil_appdispatch_libc_sync* dispatch);
int f_pthread_cond_broadcast(pthread_cond_t* cond,
                             const struct hpcrun_foil_appdispatch_libc_sync* dispatch);
int f_pthread_cond_signal(pthread_cond_t* cond,
                          const struct hpcrun_foil_appdispatch_libc_sync* dispatch);
int f_pthread_mutex_lock(pthread_mutex_t* mutex,
                         const struct hpcrun_foil_appdispatch_libc_sync* dispatch);
int f_pthread_mutex_unlock(pthread_mutex_t* mutex,
                           const struct hpcrun_foil_appdispatch_libc_sync* dispatch);
int f_pthread_mutex_timedlock(pthread_mutex_t* mutex,
                              const struct timespec* abs_timeout,
                              const struct hpcrun_foil_appdispatch_libc_sync* dispatch);
int f_pthread_spin_lock(pthread_spinlock_t* lock,
                        const struct hpcrun_foil_appdispatch_libc_sync* dispatch);
int f_pthread_spin_unlock(pthread_spinlock_t* lock,
                          const struct hpcrun_foil_appdispatch_libc_sync* dispatch);
int f_sched_yield(const struct hpcrun_foil_appdispatch_libc_sync* dispatch);
int f_sem_wait(sem_t* sem, const struct hpcrun_foil_appdispatch_libc_sync* dispatch);
int f_sem_post(sem_t* sem, const struct hpcrun_foil_appdispatch_libc_sync* dispatch);
int f_sem_timedwait(sem_t* sem, const struct timespec* abs_timeout,
                    const struct hpcrun_foil_appdispatch_libc_sync* dispatch);

struct hpcrun_foil_appdispatch_libc_alloc;

void* f_memalign(size_t boundary, size_t bytes,
                 const struct hpcrun_foil_appdispatch_libc_alloc* dispatch);
void* f_valloc(size_t bytes, const struct hpcrun_foil_appdispatch_libc_alloc* dispatch);
void* f_malloc(size_t bytes, const struct hpcrun_foil_appdispatch_libc_alloc* dispatch);
void f_free(void* ptr, const struct hpcrun_foil_appdispatch_libc_alloc* dispatch);
void* f_realloc(void* ptr, size_t bytes,
                const struct hpcrun_foil_appdispatch_libc_alloc* dispatch);

struct hpcrun_foil_appdispatch_libc_io;

ssize_t f_read(int fd, void* buf, size_t count,
               const struct hpcrun_foil_appdispatch_libc_io* dispatch);
ssize_t f_write(int fd, const void* buf, size_t count,
                const struct hpcrun_foil_appdispatch_libc_io* dispatch);
size_t f_fread(void* ptr, size_t size, size_t count, FILE* stream,
               const struct hpcrun_foil_appdispatch_libc_io* dispatch);
size_t f_fwrite(const void* ptr, size_t size, size_t count, FILE* stream,
                const struct hpcrun_foil_appdispatch_libc_io* dispatch);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HPCRUN_FOIL_LIBC_H
