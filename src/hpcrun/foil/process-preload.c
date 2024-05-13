// SPDX-FileCopyrightText: 2002-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "foil.h"
#include "../libmonitor/monitor.h"
#include <unistd.h>
#include <stdarg.h>

HPCRUN_EXPOSED int __libc_start_main(START_MAIN_PARAM_LIST) {
  LOOKUP_FOIL_BASE(base, libc_start_main);
#ifdef HOST_CPU_PPC
  return base(argc, argv, envp, auxp, rtld_fini, stinfo, stack_end);
#else
  return base(main, argc, argv, init, fini, rtld_fini, stack_end);
#endif
}

HPCRUN_EXPOSED pid_t fork() {
  LOOKUP_FOIL_BASE(base, fork);
  return base();
}

HPCRUN_EXPOSED pid_t vfork() {
  LOOKUP_FOIL_BASE(base, vfork);
  return base();
}

HPCRUN_EXPOSED pid_t execl(const char* path, const char* arg0, ...) {
  LOOKUP_FOIL_BASE(base, execv);
  va_list args;

  // Count how many arguments we will be passing total
  int argc = 0;
  va_start(args, arg0);
  for(const char* arg = arg0; arg != NULL; arg = va_arg(args, char*)) ++argc;
  va_end(args);

  // Copy the varargs to stack memory for passing through execv.
  // We can't dynamically malloc here since execl is supposed to be async-signal-safe.
  // So we instead use a dynamic stack allocation and hope there's enough stack memory.
  char* argv[argc + 1];
  size_t i = 0;
  va_start(args, arg0);
  for(char* arg = (char*)arg0; arg != NULL; arg = va_arg(args, char*)) {
    argv[i++] = arg;
  }
  argv[i] = NULL;
  assert(i == argc + 1);

  return base(path, argv);
}

HPCRUN_EXPOSED pid_t execlp(const char* file, const char* arg0, ...) {
  LOOKUP_FOIL_BASE(base, execvp);
  va_list args;

  // Count how many arguments we will be passing total
  int argc = 0;
  va_start(args, arg0);
  for(const char* arg = arg0; arg != NULL; arg = va_arg(args, char*)) ++argc;
  va_end(args);

  // Copy the varargs to stack memory for passing through execv.
  // We can't dynamically malloc here since execl is supposed to be async-signal-safe.
  // So we instead use a dynamic stack allocation and hope there's enough stack memory.
  char* argv[argc + 1];
  size_t i = 0;
  va_start(args, arg0);
  for(char* arg = (char*)arg0; arg != NULL; arg = va_arg(args, char*)) {
    argv[i++] = arg;
  }
  argv[i] = NULL;
  assert(i == argc + 1);
  va_end(args);

  return base(file, argv);
}

HPCRUN_EXPOSED pid_t execle(const char* path, const char* arg0, ...) {
  LOOKUP_FOIL_BASE(base, execve);
  va_list args;

  // Count how many arguments we will be passing total
  int argc = 0;
  va_start(args, arg0);
  for(const char* arg = arg0; arg != NULL; arg = va_arg(args, char*)) ++argc;
  va_end(args);

  // Copy the varargs to stack memory for passing through execv.
  // We can't dynamically malloc here since execl is supposed to be async-signal-safe.
  // So we instead use a dynamic stack allocation and hope there's enough stack memory.
  char* argv[argc + 1];
  size_t i = 0;
  va_start(args, arg0);
  for(char* arg = (char*)arg0; arg != NULL; arg = va_arg(args, char*)) {
    argv[i++] = arg;
  }
  argv[i] = NULL;
  assert(i == argc + 1);

  char* const* envp = va_arg(args, char* const*);
  va_end(args);

  return base(path, argv, envp);
}

HPCRUN_EXPOSED int execv(const char* path, char *const argv[]) {
  LOOKUP_FOIL_BASE(base, execv);
  return base(path, argv);
}

HPCRUN_EXPOSED int execvp(const char* path, char *const argv[]) {
  LOOKUP_FOIL_BASE(base, execvp);
  return base(path, argv);
}

HPCRUN_EXPOSED int execve(const char* path, char *const argv[], char *const envp[]) {
  LOOKUP_FOIL_BASE(base, execve);
  return base(path, argv, envp);
}

HPCRUN_EXPOSED int system(const char* command) {
  LOOKUP_FOIL_BASE(base, system);
  return base(command);
}

HPCRUN_EXPOSED void exit(int status) {
  LOOKUP_FOIL_BASE(base, exit);
  base(status);
  __builtin_unreachable();
}

HPCRUN_EXPOSED void _exit(int status) {
  LOOKUP_FOIL_BASE(base, _exit);
  base(status);
  __builtin_unreachable();
}

HPCRUN_EXPOSED void _Exit(int status) {
  LOOKUP_FOIL_BASE(base, _Exit);
  base(status);
  __builtin_unreachable();
}
