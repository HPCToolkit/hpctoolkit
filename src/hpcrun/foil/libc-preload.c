// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "common-preload.h"
#include "common.h"
#include "libc-private.h"

#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdlib.h>
#include <threads.h>

static struct hpcrun_foil_appdispatch_libc dispatch_val;

static void init_dispatch() {
  dispatch_val = (struct hpcrun_foil_appdispatch_libc){
      .sigaction = foil_dlsym("sigaction"),
      .sigprocmask = foil_dlsym("sigprocmask"),
      .pthread_self = foil_dlsym("pthread_self"),
      .pthread_setcancelstate = foil_dlsym("pthread_setcancelstate"),
      .pthread_kill = foil_dlsym("pthread_kill"),
      .uulibc_start_main = foil_dlsym("__libc_start_main"),
      .exit = foil_dlsym("exit"),
      .uexit = foil_dlsym("_exit"),
      .fork = foil_dlsym("fork"),
      .execv = foil_dlsym("execv"),
      .execvp = foil_dlsym("execvp"),
      .execve = foil_dlsym("execve"),
      .sigwait = foil_dlsym("sigwait"),
      .sigwaitinfo = foil_dlsym("sigwaitinfo"),
      .sigtimedwait = foil_dlsym("sigtimedwait"),
      .pthread_create = foil_dlsym("pthread_create"),
      .pthread_exit = foil_dlsym("pthread_exit"),
      .pthread_sigmask = foil_dlsym("pthread_sigmask"),
      .pthread_attr_init = foil_dlsym("pthread_attr_init"),
      .pthread_attr_destroy = foil_dlsym("pthread_attr_destroy"),
      .pthread_attr_getstacksize = foil_dlsym("pthread_attr_getstacksize"),
      .pthread_attr_setstacksize = foil_dlsym("pthread_attr_setstacksize"),
      .system = foil_dlsym("system"),
      .select = foil_dlsym("select"),
      .pselect = foil_dlsym("pselect"),
      .poll = foil_dlsym("poll"),
      .ppoll = foil_dlsym("ppoll"),
  };
}

static const struct hpcrun_foil_appdispatch_libc* dispatch() {
  static once_flag once = ONCE_FLAG_INIT;
  call_once(&once, init_dispatch);
  return &dispatch_val;
}

__attribute__((constructor)) void init() {
  hpcrun_foil_fetch_hooks_libc_dl()->init(dispatch());
}

// NOLINTNEXTLINE(bugprone-reserved-identifier)
HPCRUN_EXPOSED_API int __libc_start_main(
#ifdef HOST_CPU_PPC
    int argc, char** argv, char** envp, void* auxp, void (*rtld_fini)(void),
    void** stinfo, void* stack_end
#else /* default __libc_start_main() args */
    int (*main)(int, char**, char**), int argc, char** argv, void (*init)(void),
    void (*fini)(void), void (*rtld_fini)(void), void* stack_end
#endif
) {
#ifdef HOST_CPU_PPC
  return hpcrun_foil_fetch_hooks_libc_dl()->uulibc_start_main(
      argc, argv, envp, auxp, rtld_fini, stinfo, stack_end, dispatch());
#else
  return hpcrun_foil_fetch_hooks_libc_dl()->uulibc_start_main(
      main, argc, argv, init, fini, rtld_fini, stack_end, dispatch());
#endif
}

HPCRUN_EXPOSED_API pid_t fork() {
  return hpcrun_foil_fetch_hooks_libc_dl()->fork(dispatch());
}

HPCRUN_EXPOSED_API pid_t vfork() {
  return hpcrun_foil_fetch_hooks_libc_dl()->vfork(dispatch());
}

/// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
HPCRUN_EXPOSED_API pid_t execl(const char* path, const char* arg0, ...) {
  const struct hpcrun_foil_appdispatch_libc* d = dispatch();
  const struct hpcrun_foil_hookdispatch_libc* h = hpcrun_foil_fetch_hooks_libc_dl();
  va_list args;

  // Count how many arguments we will be passing total
  int argc = 0;
  va_start(args, arg0);
  for (const char* arg = arg0; arg != NULL; arg = va_arg(args, char*))
    ++argc;
  va_end(args);

  // Copy the varargs to stack memory for passing through execv.
  // We can't dynamically malloc here since execl is supposed to be async-signal-safe.
  // So we instead use a dynamic stack allocation and hope there's enough stack memory.
  char* argv[argc + 1];
  size_t i = 0;
  va_start(args, arg0);
  for (char* arg = (char*)arg0; arg != NULL; arg = va_arg(args, char*)) {
    argv[i++] = arg;
  }
  argv[i] = NULL;
  assert(i == argc + 1);

  return h->execv(path, argv, d);
}

/// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
HPCRUN_EXPOSED_API pid_t execlp(const char* file, const char* arg0, ...) {
  const struct hpcrun_foil_appdispatch_libc* d = dispatch();
  const struct hpcrun_foil_hookdispatch_libc* h = hpcrun_foil_fetch_hooks_libc_dl();
  va_list args;

  // Count how many arguments we will be passing total
  int argc = 0;
  va_start(args, arg0);
  for (const char* arg = arg0; arg != NULL; arg = va_arg(args, char*))
    ++argc;
  va_end(args);

  // Copy the varargs to stack memory for passing through execv.
  // We can't dynamically malloc here since execl is supposed to be async-signal-safe.
  // So we instead use a dynamic stack allocation and hope there's enough stack memory.
  char* argv[argc + 1];
  size_t i = 0;
  va_start(args, arg0);
  for (char* arg = (char*)arg0; arg != NULL; arg = va_arg(args, char*)) {
    argv[i++] = arg;
  }
  argv[i] = NULL;
  assert(i == argc + 1);
  va_end(args);

  return h->execvp(file, argv, d);
}

/// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
HPCRUN_EXPOSED_API pid_t execle(const char* path, const char* arg0, ...) {
  const struct hpcrun_foil_appdispatch_libc* d = dispatch();
  const struct hpcrun_foil_hookdispatch_libc* h = hpcrun_foil_fetch_hooks_libc_dl();
  va_list args;

  // Count how many arguments we will be passing total
  int argc = 0;
  va_start(args, arg0);
  for (const char* arg = arg0; arg != NULL; arg = va_arg(args, char*))
    ++argc;
  va_end(args);

  // Copy the varargs to stack memory for passing through execv.
  // We can't dynamically malloc here since execl is supposed to be async-signal-safe.
  // So we instead use a dynamic stack allocation and hope there's enough stack memory.
  char* argv[argc + 1];
  size_t i = 0;
  va_start(args, arg0);
  for (char* arg = (char*)arg0; arg != NULL; arg = va_arg(args, char*)) {
    argv[i++] = arg;
  }
  argv[i] = NULL;
  assert(i == argc + 1);

  char* const* envp = va_arg(args, char* const*);
  va_end(args);

  return h->execve(path, argv, envp, d);
}

HPCRUN_EXPOSED_API int execv(const char* path, char* const argv[]) {
  return hpcrun_foil_fetch_hooks_libc_dl()->execv(path, argv, dispatch());
}

HPCRUN_EXPOSED_API int execvp(const char* path, char* const argv[]) {
  return hpcrun_foil_fetch_hooks_libc_dl()->execvp(path, argv, dispatch());
}

HPCRUN_EXPOSED_API int execve(const char* path, char* const argv[],
                              char* const envp[]) {
  return hpcrun_foil_fetch_hooks_libc_dl()->execve(path, argv, envp, dispatch());
}

HPCRUN_EXPOSED_API int system(const char* command) {
  return hpcrun_foil_fetch_hooks_libc_dl()->system(command, dispatch());
}

HPCRUN_EXPOSED_API void exit(int status) {
  hpcrun_foil_fetch_hooks_libc_dl()->exit(status, dispatch());
  __builtin_unreachable();
}

HPCRUN_EXPOSED_API void _exit(int status) {
  hpcrun_foil_fetch_hooks_libc_dl()->uexit(status, dispatch());
  __builtin_unreachable();
}

HPCRUN_EXPOSED_API void _Exit(int status) {
  hpcrun_foil_fetch_hooks_libc_dl()->uExit(status, dispatch());
  __builtin_unreachable();
}

HPCRUN_EXPOSED_API int pthread_create(pthread_t* t, const pthread_attr_t* a,
                                      void* (*s)(void*), void* d) {
  return hpcrun_foil_fetch_hooks_libc_dl()->pthread_create(
      t, a, s, d, __builtin_return_address(0), dispatch());
}

HPCRUN_EXPOSED_API void pthread_exit(void* data) {
  hpcrun_foil_fetch_hooks_libc_dl()->pthread_exit(data, dispatch());
  __builtin_unreachable();
}

HPCRUN_EXPOSED_API int pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset) {
  return hpcrun_foil_fetch_hooks_libc_dl()->pthread_sigmask(how, set, oldset,
                                                            dispatch());
}

HPCRUN_EXPOSED_API int sigaction(int sig, const struct sigaction* act,
                                 struct sigaction* oldact) {
  return hpcrun_foil_fetch_hooks_libc_dl()->sigaction(sig, act, oldact, dispatch());
}

HPCRUN_EXPOSED_API void (*signal(int sig, void (*handler)(int)))(int) {
  return hpcrun_foil_fetch_hooks_libc_dl()->signal(sig, handler, dispatch());
}

HPCRUN_EXPOSED_API __sighandler_t __sysv_signal(int signo, __sighandler_t handler) {
  return signal(signo, handler);
}

HPCRUN_EXPOSED_API int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
  return hpcrun_foil_fetch_hooks_libc_dl()->sigprocmask(how, set, oldset, dispatch());
}

HPCRUN_EXPOSED_API int sigwait(const sigset_t* set, int* sig) {
  return hpcrun_foil_fetch_hooks_libc_dl()->sigwait(set, sig, dispatch());
}

HPCRUN_EXPOSED_API int sigwaitinfo(const sigset_t* set, siginfo_t* info) {
  return hpcrun_foil_fetch_hooks_libc_dl()->sigwaitinfo(set, info, dispatch());
}

HPCRUN_EXPOSED_API int sigtimedwait(const sigset_t* set, siginfo_t* info,
                                    const struct timespec* timeout) {
  return hpcrun_foil_fetch_hooks_libc_dl()->sigtimedwait(set, info, timeout,
                                                         dispatch());
}

static inline void tspec_add(struct timespec* result, const struct timespec* a,
                             const struct timespec* b) {
  result->tv_sec = a->tv_sec + b->tv_sec;
  result->tv_nsec = a->tv_nsec + b->tv_nsec;

  if (result->tv_nsec >= 1000000000) {
    result->tv_sec++;
    result->tv_nsec -= 1000000000;
  }
}

static inline void tspec_sub(struct timespec* result, const struct timespec* a,
                             const struct timespec* b) {
  result->tv_sec = a->tv_sec - b->tv_sec;
  result->tv_nsec = a->tv_nsec - b->tv_nsec;

  if (result->tv_nsec < 0) {
    result->tv_sec--;
    result->tv_nsec += 1000000000;
  }
}

HPCRUN_EXPOSED_API int select(int nfds, fd_set* read_fds, fd_set* write_fds,
                              fd_set* except_fds, struct timeval* timeout) {
  const struct hpcrun_foil_appdispatch_libc* d = dispatch();

  int retval;
  int incoming_errno = errno; // save incoming errno

  for (;;) {
    retval = d->select(nfds, read_fds, write_fds, except_fds, timeout);

    if (retval == -1) {
      if (errno == EINTR) {
        // restart on EINTR
        errno = incoming_errno; // restore incoming errno
        continue;
      }
    }

    // return otherwise
    break;
  }

  return retval;
}

HPCRUN_EXPOSED_API int pselect(int nfds, fd_set* readfds, fd_set* writefds,
                               fd_set* exceptfds, const struct timespec* init_timeout,
                               const sigset_t* sigmask) {
  const struct hpcrun_foil_appdispatch_libc* d = dispatch();

  struct timespec end = {0, 0}, now, my_timeout, *timeout_ptr;
  int init_errno = errno;
  int ret;

  //
  // update timeout only if non-NULL and > 0, otherwise just pass on
  // the original value.
  //
  int update_timeout = (init_timeout != NULL) &&
                       (init_timeout->tv_sec > 0 ||
                        (init_timeout->tv_sec == 0 && init_timeout->tv_nsec > 0));

  if (update_timeout) {
    clock_gettime(CLOCK_REALTIME, &now);
    tspec_add(&end, &now, init_timeout);
    my_timeout = *init_timeout;
    timeout_ptr = &my_timeout;
  } else {
    timeout_ptr = (struct timespec*)init_timeout;
  }

  for (;;) {
    ret = d->pselect(nfds, readfds, writefds, exceptfds, timeout_ptr, sigmask);

    if (!(ret < 0 && errno == EINTR)) {
      // normal (non-signal) return
      break;
    }
    errno = init_errno;

    // adjust timeout and restart syscall
    if (update_timeout) {
      clock_gettime(CLOCK_REALTIME, &now);
      tspec_sub(&my_timeout, &end, &now);

      //
      // if timeout has expired, then call one more time with timeout
      // zero so the kernel sets ret and errno correctly.
      //
      if (my_timeout.tv_sec < 0 || my_timeout.tv_nsec < 0) {
        my_timeout.tv_sec = 0;
        my_timeout.tv_nsec = 0;
      }
    }
  }

  return ret;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
HPCRUN_EXPOSED_API int poll(struct pollfd* fds, nfds_t nfds, int init_timeout) {
  const struct hpcrun_foil_appdispatch_libc* d = dispatch();

  struct timespec start, now;
  int incoming_errno = errno; // save incoming errno
  int ret;

  if (init_timeout > 0) {
    clock_gettime(CLOCK_REALTIME, &start);
  }

  int timeout = init_timeout;

  for (;;) {
    // call the libc poll operation
    ret = d->poll(fds, nfds, timeout);

    if (!(ret < 0 && errno == EINTR)) {
      // normal (non-signal) return
      break;
    }
    errno = incoming_errno;

    // adjust timeout and restart syscall
    if (init_timeout > 0) {
      clock_gettime(CLOCK_REALTIME, &now);

      timeout = init_timeout - 1000 * (now.tv_sec - start.tv_sec) -
                (now.tv_nsec - start.tv_nsec) / 1000000;

      //
      // if timeout has expired, then call one more time with timeout
      // zero so the kernel sets ret and errno correctly.
      //
      if (timeout < 0) {
        timeout = 0;
      }
    }
  }

  return ret;
}

HPCRUN_EXPOSED_API int ppoll(struct pollfd* fds, nfds_t nfds,
                             const struct timespec* init_timeout,
                             const sigset_t* sigmask) {
  const struct hpcrun_foil_appdispatch_libc* d = dispatch();

  struct timespec end = {0, 0}, now, my_timeout, *timeout_ptr;
  int init_errno = errno;
  int ret;

  //
  // update timeout only if non-NULL and > 0, otherwise just pass on
  // the original value.
  //
  int update_timeout = (init_timeout != NULL) &&
                       (init_timeout->tv_sec > 0 ||
                        (init_timeout->tv_sec == 0 && init_timeout->tv_nsec > 0));

  if (update_timeout) {
    clock_gettime(CLOCK_REALTIME, &now);
    tspec_add(&end, &now, init_timeout);
    my_timeout = *init_timeout;
    timeout_ptr = &my_timeout;
  } else {
    timeout_ptr = (struct timespec*)init_timeout;
  }

  for (;;) {
    ret = d->ppoll(fds, nfds, timeout_ptr, sigmask);

    if (!(ret < 0 && errno == EINTR)) {
      // normal (non-signal) return
      break;
    }
    errno = init_errno;

    // adjust timeout and restart syscall
    if (update_timeout) {
      clock_gettime(CLOCK_REALTIME, &now);
      tspec_sub(&my_timeout, &end, &now);

      //
      // if timeout has expired, then call one more time with timeout
      // zero so the kernel sets ret and errno correctly.
      //
      if (my_timeout.tv_sec < 0 || my_timeout.tv_nsec < 0) {
        my_timeout.tv_sec = 0;
        my_timeout.tv_nsec = 0;
      }
    }
  }

  return ret;
}
