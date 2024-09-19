// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#include "libc.h"

#include "../libmonitor/monitor.h"
#include "../sample-sources/io-over.h"
#include "../sample-sources/memleak-overrides.h"
#include "../sample-sources/pthread-blame-overrides.h"
#include "common.h"
#include "libc-private.h"

#include <assert.h>
#include <dlfcn.h>
#include <stdatomic.h>

static void init(const struct hpcrun_foil_appdispatch_libc* dispatch) {
  monitor_initialize();
}

const struct hpcrun_foil_hookdispatch_libc* hpcrun_foil_fetch_hooks_libc() {
  static const struct hpcrun_foil_hookdispatch_libc hooks = {
      .init = init,
      .uulibc_start_main = hpcrun_libc_start_main,
      .fork = hpcrun_fork,
      .vfork = hpcrun_vfork,
      .execv = hpcrun_execv,
      .execvp = hpcrun_execvp,
      .execve = hpcrun_execve,
      .system = hpcrun_system,
      .exit = hpcrun_exit,
      .uexit = hpcrun_uexit,
      .uExit = hpcrun_uExit,
      .sigaction = hpcrun_sigaction,
      .signal = hpcrun_signal,
      .sigprocmask = hpcrun_sigprocmask,
      .sigwait = hpcrun_sigwait,
      .sigwaitinfo = hpcrun_sigwaitinfo,
      .sigtimedwait = hpcrun_sigtimedwait,
      .pthread_create = hpcrun_pthread_create,
      .pthread_exit = hpcrun_pthread_exit,
      .pthread_sigmask = hpcrun_pthread_sigmask,
      .pthread_cond_timedwait = hpcrun_pthread_cond_timedwait,
      .pthread_cond_wait = hpcrun_pthread_cond_wait,
      .pthread_cond_broadcast = hpcrun_pthread_cond_broadcast,
      .pthread_cond_signal = hpcrun_pthread_cond_signal,
      .pthread_mutex_lock = hpcrun_pthread_mutex_lock,
      .pthread_mutex_unlock = hpcrun_pthread_mutex_unlock,
      .pthread_mutex_timedlock = hpcrun_pthread_mutex_timedlock,
      .pthread_spin_lock = hpcrun_pthread_spin_lock,
      .pthread_spin_unlock = hpcrun_pthread_spin_unlock,
      .sched_yield = hpcrun_sched_yield,
      .sem_wait = hpcrun_sem_wait,
      .sem_post = hpcrun_sem_post,
      .sem_timedwait = hpcrun_sem_timedwait,
      .posix_memalign = hpcrun_posix_memalign,
      .memalign = hpcrun_memalign,
      .valloc = hpcrun_valloc,
      .malloc = hpcrun_malloc_intercept,
      .calloc = hpcrun_calloc,
      .free = hpcrun_free,
      .realloc = hpcrun_realloc,
      .read = hpcrun_read,
      .write = hpcrun_write,
      .fread = hpcrun_fread,
      .fwrite = hpcrun_fwrite,
  };
  return &hooks;
}

int f_uulibc_start_main(
#ifdef HOST_CPU_PPC
    int argc, char** argv, char** envp, void* auxp, void (*rtld_fini)(void),
    void** stinfo, void* stack_end,
#else
    int (*main)(int, char**, char**), int argc, char** argv, void (*init)(void),
    void (*fini)(void), void (*rtld_fini)(void), void* stack_end,
#endif
    const struct hpcrun_foil_appdispatch_libc* dispatch) {
#ifdef HOST_CPU_PPC
  return dispatch->uulibc_start_main(argc, argv, envp, auxp, rtld_fini, stinfo,
                                     stack_end);
#else
  return dispatch->uulibc_start_main(main, argc, argv, init, fini, rtld_fini,
                                     stack_end);
#endif
}

pid_t f_fork(const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->fork();
}

int f_execv(const char* pathname, char* const argv[],
            const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->execv(pathname, argv);
}

int f_execvp(const char* file, char* const argv[],
             const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->execvp(file, argv);
}

int f_execve(const char* pathname, char* const argv[], char* const envp[],
             const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->execve(pathname, argv, envp);
}

int f_system(const char* command, const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->system(command);
}

void f_exit(int status, const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->exit(status);
}

void f_uexit(int status, const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->uexit(status);
}

int f_sigaction(int signum, const struct sigaction* restrict act,
                struct sigaction* restrict oldact,
                const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->sigaction(signum, act, oldact);
}

int f_sigprocmask(int how, const sigset_t* restrict set, sigset_t* restrict oldset,
                  const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->sigprocmask(how, set, oldset);
}

int f_sigwaitinfo(const sigset_t* set, siginfo_t* info,
                  const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->sigwaitinfo(set, info);
}

int f_sigtimedwait(const sigset_t* set, siginfo_t* info, const struct timespec* timeout,
                   const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->sigtimedwait(set, info, timeout);
}

int f_pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                     void* (*start)(void*), void* udata,
                     const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->pthread_create(thread, attr, start, udata);
}

void f_pthread_exit(void* result, const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->pthread_exit(result);
}

int f_pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset,
                      const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->pthread_sigmask(how, set, oldset);
}

int f_pthread_attr_init(pthread_attr_t* attr,
                        const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->pthread_attr_init(attr);
}

int f_pthread_attr_destroy(pthread_attr_t* attr,
                           const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->pthread_attr_destroy(attr);
}

int f_pthread_attr_getstacksize(pthread_attr_t* attr, size_t* size,
                                const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->pthread_attr_getstacksize(attr, size);
}

int f_pthread_attr_setstacksize(pthread_attr_t* attr, size_t size,
                                const struct hpcrun_foil_appdispatch_libc* dispatch) {
  return dispatch->pthread_attr_setstacksize(attr, size);
}

int f_pthread_cond_timedwait(pthread_cond_t* restrict cond,
                             pthread_mutex_t* restrict mutex,
                             const struct timespec* restrict abstime,
                             const struct hpcrun_foil_appdispatch_libc_sync* dispatch) {
  return dispatch->pthread_cond_timedwait(cond, mutex, abstime);
}

int f_pthread_cond_wait(pthread_cond_t* restrict cond, pthread_mutex_t* restrict mutex,
                        const struct hpcrun_foil_appdispatch_libc_sync* dispatch) {
  return dispatch->pthread_cond_wait(cond, mutex);
}

int f_pthread_cond_broadcast(pthread_cond_t* cond,
                             const struct hpcrun_foil_appdispatch_libc_sync* dispatch) {
  return dispatch->pthread_cond_broadcast(cond);
}

int f_pthread_cond_signal(pthread_cond_t* cond,
                          const struct hpcrun_foil_appdispatch_libc_sync* dispatch) {
  return dispatch->pthread_cond_signal(cond);
}

int f_pthread_mutex_lock(pthread_mutex_t* mutex,
                         const struct hpcrun_foil_appdispatch_libc_sync* dispatch) {
  return dispatch->pthread_mutex_lock(mutex);
}

int f_pthread_mutex_unlock(pthread_mutex_t* mutex,
                           const struct hpcrun_foil_appdispatch_libc_sync* dispatch) {
  return dispatch->pthread_mutex_unlock(mutex);
}

int f_pthread_mutex_timedlock(pthread_mutex_t* restrict mutex,
                              const struct timespec* restrict abs_timeout,
                              const struct hpcrun_foil_appdispatch_libc_sync* dispatch) {
  return dispatch->pthread_mutex_timedlock(mutex, abs_timeout);
}

int f_pthread_spin_lock(pthread_spinlock_t* lock,
                        const struct hpcrun_foil_appdispatch_libc_sync* dispatch) {
  return dispatch->pthread_spin_lock(lock);
}

int f_pthread_spin_unlock(pthread_spinlock_t* lock,
                          const struct hpcrun_foil_appdispatch_libc_sync* dispatch) {
  return dispatch->pthread_spin_unlock(lock);
}

int f_sched_yield(const struct hpcrun_foil_appdispatch_libc_sync* dispatch) {
  return dispatch->sched_yield();
}

int f_sem_wait(sem_t* sem, const struct hpcrun_foil_appdispatch_libc_sync* dispatch) {
  return dispatch->sem_wait(sem);
}

int f_sem_post(sem_t* sem, const struct hpcrun_foil_appdispatch_libc_sync* dispatch) {
  return dispatch->sem_post(sem);
}

int f_sem_timedwait(sem_t* sem, const struct timespec* abs_timeout,
                    const struct hpcrun_foil_appdispatch_libc_sync* dispatch) {
  return dispatch->sem_timedwait(sem, abs_timeout);
}

void* f_memalign(size_t boundary, size_t bytes,
                 const struct hpcrun_foil_appdispatch_libc_alloc* dispatch) {
  return dispatch->memalign(boundary, bytes);
}

void* f_valloc(size_t bytes,
               const struct hpcrun_foil_appdispatch_libc_alloc* dispatch) {
  return dispatch->valloc(bytes);
}

void* f_malloc(size_t bytes,
               const struct hpcrun_foil_appdispatch_libc_alloc* dispatch) {
  return dispatch->malloc(bytes);
}

void f_free(void* ptr, const struct hpcrun_foil_appdispatch_libc_alloc* dispatch) {
  return dispatch->free(ptr);
}

void* f_realloc(void* ptr, size_t bytes,
                const struct hpcrun_foil_appdispatch_libc_alloc* dispatch) {
  return dispatch->realloc(ptr, bytes);
}

ssize_t f_read(int fd, void* buf, size_t count,
               const struct hpcrun_foil_appdispatch_libc_io* dispatch) {
  return dispatch->read(fd, buf, count);
}

ssize_t f_write(int fd, const void* buf, size_t count,
                const struct hpcrun_foil_appdispatch_libc_io* dispatch) {
  return dispatch->write(fd, buf, count);
}

size_t f_fread(void* ptr, size_t size, size_t count, FILE* stream,
               const struct hpcrun_foil_appdispatch_libc_io* dispatch) {
  return dispatch->fread(ptr, size, count, stream);
}

size_t f_fwrite(const void* ptr, size_t size, size_t count, FILE* stream,
                const struct hpcrun_foil_appdispatch_libc_io* dispatch) {
  return dispatch->fwrite(ptr, size, count, stream);
}
