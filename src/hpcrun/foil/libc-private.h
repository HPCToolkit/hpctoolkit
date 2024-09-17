// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_LIBC_PRIVATE_H
#define HPCRUN_FOIL_LIBC_PRIVATE_H

#include "common.h"

#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
#error This is a C-only header
#endif

struct hpcrun_foil_appdispatch_libc {
  int (*sigaction)(int, const struct sigaction*, struct sigaction*);
  int (*sigprocmask)(int, const sigset_t*, sigset_t*);
  pthread_t (*pthread_self)();
  int (*pthread_setcancelstate)(int, int*);
  int (*pthread_kill)(pthread_t, int);
  int (*uulibc_start_main)(
#ifdef HOST_CPU_PPC
      int argc, char** argv, char** envp, void* auxp, void (*rtld_fini)(void),
      void** stinfo, void* stack_end
#else /* default __libc_start_main() args */
      int (*main)(int, char**, char**), int argc, char** argv, void (*init)(void),
      void (*fini)(void), void (*rtld_fini)(void), void* stack_end
#endif
  );
  void (*exit)(int);
  void (*uexit)(int);
  pid_t (*fork)();
  int (*execv)(const char*, char* const[]);
  int (*execvp)(const char*, char* const[]);
  int (*execve)(const char*, char* const[], char* const[]);
  int (*sigwait)(const sigset_t*, int*);
  int (*sigwaitinfo)(const sigset_t*, siginfo_t*);
  int (*sigtimedwait)(const sigset_t*, siginfo_t*, const struct timespec*);
  int (*pthread_create)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
  void (*pthread_exit)(void*);
  int (*pthread_sigmask)(int, const sigset_t*, sigset_t*);
  int (*pthread_attr_init)(pthread_attr_t* attr);
  int (*pthread_attr_destroy)(pthread_attr_t* attr);
  int (*pthread_attr_getstacksize)(pthread_attr_t* attr, size_t* size);
  int (*pthread_attr_setstacksize)(pthread_attr_t* attr, size_t size);
  int (*system)(const char*);
  int (*select)(int, fd_set* restrict, fd_set* restrict, fd_set* restrict,
                struct timeval* restrict);
  int (*pselect)(int, fd_set* restrict, fd_set* restrict, fd_set* restrict,
                 const struct timespec* restrict, const sigset_t* restrict);
  int (*poll)(struct pollfd*, nfds_t, int);
  int (*ppoll)(struct pollfd*, nfds_t, const struct timespec*, const sigset_t*);
};

struct hpcrun_foil_appdispatch_libc_sync {
  int (*pthread_cond_timedwait)(pthread_cond_t* restrict, pthread_mutex_t* restrict,
                                const struct timespec* restrict);
  int (*pthread_cond_wait)(pthread_cond_t* restrict, pthread_mutex_t* restrict);
  int (*pthread_cond_broadcast)(pthread_cond_t*);
  int (*pthread_cond_signal)(pthread_cond_t*);
  int (*pthread_mutex_lock)(pthread_mutex_t*);
  int (*pthread_mutex_unlock)(pthread_mutex_t*);
  int (*pthread_mutex_timedlock)(pthread_mutex_t* restrict,
                                 const struct timespec* restrict);
  int (*pthread_spin_lock)(pthread_spinlock_t*);
  int (*pthread_spin_unlock)(pthread_spinlock_t*);
  int (*sched_yield)();
  int (*sem_wait)(sem_t*);
  int (*sem_post)(sem_t*);
  int (*sem_timedwait)(sem_t*, const struct timespec*);
};

struct hpcrun_foil_appdispatch_libc_alloc {
  void* (*memalign)(size_t, size_t);
  void* (*valloc)(size_t);
  void* (*malloc)(size_t);
  void* (*calloc)(size_t, size_t);
  void (*free)(void*);
  void* (*realloc)(void*, size_t);
};

struct hpcrun_foil_appdispatch_libc_io {
  ssize_t (*read)(int, void*, size_t);
  ssize_t (*write)(int, const void*, size_t);
  size_t (*fread)(void*, size_t, size_t, FILE*);
  size_t (*fwrite)(const void*, size_t, size_t, FILE*);
};

struct hpcrun_foil_hookdispatch_libc {
  void (*init)(const struct hpcrun_foil_appdispatch_libc*);
  int (*uulibc_start_main)(
#ifdef HOST_CPU_PPC
      int argc, char** argv, char** envp, void* auxp, void (*rtld_fini)(void),
      void** stinfo, void* stack_end,
#else
      int (*main)(int, char**, char**), int argc, char** argv, void (*init)(void),
      void (*fini)(void), void (*rtld_fini)(void), void* stack_end,
#endif
      const struct hpcrun_foil_appdispatch_libc*);
  pid_t (*fork)(const struct hpcrun_foil_appdispatch_libc*);
  pid_t (*vfork)(const struct hpcrun_foil_appdispatch_libc*);
  int (*execv)(const char*, char* const[], const struct hpcrun_foil_appdispatch_libc*);
  int (*execvp)(const char*, char* const[], const struct hpcrun_foil_appdispatch_libc*);
  int (*execve)(const char*, char* const[], char* const[],
                const struct hpcrun_foil_appdispatch_libc*);
  int (*system)(const char*, const struct hpcrun_foil_appdispatch_libc*);
  void (*exit)(int, const struct hpcrun_foil_appdispatch_libc*);
  void (*uexit)(int, const struct hpcrun_foil_appdispatch_libc*);
  void (*uExit)(int, const struct hpcrun_foil_appdispatch_libc*);
  int (*pthread_create)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*,
                        void*, const struct hpcrun_foil_appdispatch_libc*);
  void (*pthread_exit)(void*, const struct hpcrun_foil_appdispatch_libc*);
  int (*pthread_sigmask)(int, const sigset_t*, sigset_t*,
                         const struct hpcrun_foil_appdispatch_libc*);
  int (*sigaction)(int, const struct sigaction*, struct sigaction*,
                   const struct hpcrun_foil_appdispatch_libc*);
  void (*(*signal)(int, void (*)(int),
                   const struct hpcrun_foil_appdispatch_libc*))(int);
  int (*sigprocmask)(int, const sigset_t*, sigset_t*,
                     const struct hpcrun_foil_appdispatch_libc*);
  int (*sigwait)(const sigset_t*, int*, const struct hpcrun_foil_appdispatch_libc*);
  int (*sigwaitinfo)(const sigset_t*, siginfo_t*,
                     const struct hpcrun_foil_appdispatch_libc*);
  int (*sigtimedwait)(const sigset_t*, siginfo_t*, const struct timespec*,
                      const struct hpcrun_foil_appdispatch_libc*);
  int (*pthread_cond_timedwait)(pthread_cond_t* restrict, pthread_mutex_t* restrict,
                                const struct timespec* restrict,
                                const struct hpcrun_foil_appdispatch_libc_sync*);
  int (*pthread_cond_wait)(pthread_cond_t* restrict, pthread_mutex_t* restrict,
                           const struct hpcrun_foil_appdispatch_libc_sync*);
  int (*pthread_cond_broadcast)(pthread_cond_t*,
                                const struct hpcrun_foil_appdispatch_libc_sync*);
  int (*pthread_cond_signal)(pthread_cond_t*,
                             const struct hpcrun_foil_appdispatch_libc_sync*);
  int (*pthread_mutex_lock)(pthread_mutex_t*,
                            const struct hpcrun_foil_appdispatch_libc_sync*);
  int (*pthread_mutex_unlock)(pthread_mutex_t*,
                              const struct hpcrun_foil_appdispatch_libc_sync*);
  int (*pthread_mutex_timedlock)(pthread_mutex_t* restrict,
                                 const struct timespec* restrict,
                                 const struct hpcrun_foil_appdispatch_libc_sync*);
  int (*pthread_spin_lock)(pthread_spinlock_t*,
                           const struct hpcrun_foil_appdispatch_libc_sync*);
  int (*pthread_spin_unlock)(pthread_spinlock_t*,
                             const struct hpcrun_foil_appdispatch_libc_sync*);
  int (*sched_yield)(const struct hpcrun_foil_appdispatch_libc_sync*);
  int (*sem_wait)(sem_t*, const struct hpcrun_foil_appdispatch_libc_sync*);
  int (*sem_post)(sem_t*, const struct hpcrun_foil_appdispatch_libc_sync*);
  int (*sem_timedwait)(sem_t*, const struct timespec*,
                       const struct hpcrun_foil_appdispatch_libc_sync*);
  int (*posix_memalign)(void**, size_t, size_t,
                        const struct hpcrun_foil_appdispatch_libc_alloc*);
  void* (*memalign)(size_t, size_t, const struct hpcrun_foil_appdispatch_libc_alloc*);
  void* (*valloc)(size_t, const struct hpcrun_foil_appdispatch_libc_alloc*);
  void* (*malloc)(size_t, const struct hpcrun_foil_appdispatch_libc_alloc*);
  void* (*calloc)(size_t, size_t, const struct hpcrun_foil_appdispatch_libc_alloc*);
  void (*free)(void*, const struct hpcrun_foil_appdispatch_libc_alloc*);
  void* (*realloc)(void*, size_t, const struct hpcrun_foil_appdispatch_libc_alloc*);
  ssize_t (*read)(int, void*, size_t, const struct hpcrun_foil_appdispatch_libc_io*);
  ssize_t (*write)(int, const void*, size_t,
                   const struct hpcrun_foil_appdispatch_libc_io*);
  size_t (*fread)(void*, size_t, size_t, FILE*,
                  const struct hpcrun_foil_appdispatch_libc_io*);
  size_t (*fwrite)(const void*, size_t, size_t, FILE*,
                   const struct hpcrun_foil_appdispatch_libc_io*);
};

#endif // HPCRUN_FOIL_LIBC_PRIVATE_H
