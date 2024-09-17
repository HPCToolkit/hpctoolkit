// SPDX-FileCopyrightText: 2007-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Internal shared declarations.
 */

#ifndef _MONITOR_COMMON_H_
#define _MONITOR_COMMON_H_


#include <sys/types.h>
#include <dlfcn.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#ifndef RTLD_NEXT
#define RTLD_NEXT  ((void *) -1l)
#endif

/*
 *  Maximum signal number for array bounds.
 */
#ifdef NSIG
#define MONITOR_NSIG  (NSIG)
#else
#define MONITOR_NSIG   128
#endif

#define TRUE   1
#define FALSE  0
#define SUCCESS   0
#define FAILURE  -1

#define MONITOR_POLL_USLEEP_TIME  100000
#define MONITOR_SIG_BUF_SIZE   500

/*
 *  Format (fmt) must be a string constant in these macros.  Some
 *  compilers don't accept the ##__VA_ARGS__ syntax for the case of
 *  empty args, so split the macros into two.
 */
#define MONITOR_DEBUG_ARGS(fmt, ...)  do {                      \
    if (monitor_debug) {                                        \
        fprintf(stderr, "monitor debug [%d,%d] %s: " fmt ,      \
                getpid(), monitor_get_thread_num(),             \
                __VA_ARGS__ );                                  \
    }                                                           \
} while (0)

#define MONITOR_WARN_ARGS(fmt, ...)  do {                       \
    fprintf(stderr, "monitor warning [%d,%d] %s: " fmt ,        \
            getpid(), monitor_get_thread_num(),                 \
            __VA_ARGS__ );                                      \
} while (0)

#define MONITOR_WARN_NO_TID_ARGS(fmt, ...)  do {                \
    fprintf(stderr, "monitor warning [%d,--] %s: " fmt ,        \
            getpid(), __VA_ARGS__ );                            \
} while (0)

#define MONITOR_ERROR_ARGS(fmt, ...)  do {                      \
    fprintf(stderr, "monitor error [%d,%d] %s: " fmt ,          \
            getpid(), monitor_get_thread_num(),                 \
            __VA_ARGS__ );                                      \
    errx(1, "%s:" fmt , __VA_ARGS__ );                          \
} while (0)

#define MONITOR_DEBUG1(fmt)      MONITOR_DEBUG_ARGS(fmt, __func__)
#define MONITOR_DEBUG(fmt, ...)  MONITOR_DEBUG_ARGS(fmt, __func__, __VA_ARGS__)

#define MONITOR_WARN1(fmt)      MONITOR_WARN_ARGS(fmt, __func__)
#define MONITOR_WARN(fmt, ...)  MONITOR_WARN_ARGS(fmt, __func__, __VA_ARGS__)
#define MONITOR_WARN_NO_TID(fmt, ...)  MONITOR_WARN_NO_TID_ARGS(fmt, __func__, __VA_ARGS__)

#define MONITOR_ERROR1(fmt)      MONITOR_ERROR_ARGS(fmt, __func__)
#define MONITOR_ERROR(fmt, ...)  MONITOR_ERROR_ARGS(fmt, __func__, __VA_ARGS__)

#define MONITOR_ASM_LABEL(name)         \
    asm volatile (".globl " #name );    \
    asm volatile ( #name ":" )

#define MONITOR_RUN_ONCE(var)                           \
    static char monitor_has_run_##var = 0;              \
    if ( monitor_has_run_##var ) { return; }            \
    monitor_has_run_##var = 1

extern int monitor_debug;

void monitor_early_init(void);
void monitor_fork_init(void);
void monitor_signal_init();
void monitor_begin_process_fcn(void *, int);
void monitor_end_process_fcn(int);
void monitor_end_library_fcn(void);
void monitor_thread_shootdown(void);
int  monitor_shootdown_signal();
int  monitor_sigwait_handler(int, siginfo_t *, void *);
void monitor_remove_client_signals(sigset_t *, int);
int  monitor_sigset_string(char *, int, const sigset_t *);
int  monitor_signal_list_string(char *, int, int *);
void monitor_get_main_args(int *, char ***, char ***);
int  monitor_in_main_start_func_wide(void *);
int  monitor_in_main_start_func_narrow(void *);
void monitor_set_mpi_size_rank(int, int);
int  monitor_mpi_init_count(int);
int  monitor_mpi_fini_count(int);

#endif  /* ! _MONITOR_COMMON_H_ */
