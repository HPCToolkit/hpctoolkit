/*
 *  Internal shared declarations.
 *
 *  Copyright (c) 2007-2023, Rice University.
 *  All rights reserved.
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  
 *  * Neither the name of Rice University (RICE) nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *  
 *  This software is provided by RICE and contributors "as is" and any
 *  express or implied warranties, including, but not limited to, the
 *  implied warranties of merchantability and fitness for a particular
 *  purpose are disclaimed. In no event shall RICE or contributors be
 *  liable for any direct, indirect, incidental, special, exemplary, or
 *  consequential damages (including, but not limited to, procurement of
 *  substitute goods or services; loss of use, data, or profits; or
 *  business interruption) however caused and on any theory of liability,
 *  whether in contract, strict liability, or tort (including negligence
 *  or otherwise) arising in any way out of the use of this software, even
 *  if advised of the possibility of such damage.
 *
 *  $Id$
 */

#ifndef _MONITOR_COMMON_H_
#define _MONITOR_COMMON_H_

#include "config.h"

#include <sys/types.h>
#ifdef MONITOR_DYNAMIC
#include <dlfcn.h>
#endif
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

/*
 *  Exactly one of MONITOR_STATIC and MONITOR_DYNAMIC must be defined,
 *  preferably on the compile line (to allow for multiple builds of
 *  the same file).
 */
#if !defined(MONITOR_STATIC) && !defined(MONITOR_DYNAMIC)
#error Must define MONITOR_STATIC or MONITOR_DYNAMIC.
#endif
#if defined(MONITOR_STATIC) && defined(MONITOR_DYNAMIC)
#error Must not define both MONITOR_STATIC and MONITOR_DYNAMIC.
#endif

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
#define MONITOR_DEBUG_ARGS(fmt, ...)  do {			\
    if (monitor_debug) {					\
	fprintf(stderr, "monitor debug [%d,%d] %s: " fmt ,	\
		getpid(), monitor_get_thread_num(),		\
		__VA_ARGS__ );					\
    }							       	\
} while (0)

#define MONITOR_WARN_ARGS(fmt, ...)  do {			\
    fprintf(stderr, "monitor warning [%d,%d] %s: " fmt ,	\
	    getpid(), monitor_get_thread_num(),			\
	    __VA_ARGS__ );					\
} while (0)

#define MONITOR_WARN_NO_TID_ARGS(fmt, ...)  do {		\
    fprintf(stderr, "monitor warning [%d,--] %s: " fmt ,	\
	    getpid(), __VA_ARGS__ );				\
} while (0)

#define MONITOR_ERROR_ARGS(fmt, ...)  do {			\
    fprintf(stderr, "monitor error [%d,%d] %s: " fmt ,		\
	    getpid(), monitor_get_thread_num(),			\
	    __VA_ARGS__ );					\
    errx(1, "%s:" fmt , __VA_ARGS__ );				\
} while (0)

#define MONITOR_DEBUG1(fmt)      MONITOR_DEBUG_ARGS(fmt, __func__)
#define MONITOR_DEBUG(fmt, ...)  MONITOR_DEBUG_ARGS(fmt, __func__, __VA_ARGS__)

#define MONITOR_WARN1(fmt)      MONITOR_WARN_ARGS(fmt, __func__)
#define MONITOR_WARN(fmt, ...)  MONITOR_WARN_ARGS(fmt, __func__, __VA_ARGS__)
#define MONITOR_WARN_NO_TID(fmt, ...)  MONITOR_WARN_NO_TID_ARGS(fmt, __func__, __VA_ARGS__)

#define MONITOR_ERROR1(fmt)      MONITOR_ERROR_ARGS(fmt, __func__)
#define MONITOR_ERROR(fmt, ...)  MONITOR_ERROR_ARGS(fmt, __func__, __VA_ARGS__)

void *monitor_dlsym(const char *symbol);

#define MONITOR_REQUIRE_DLSYM(var, name)  do {		\
    if (var == NULL) {					\
        var = monitor_dlsym(name);                      \
    }							\
} while (0)

#ifdef MONITOR_STATIC
#define MONITOR_WRAP_NAME_HELP(name)  __wrap_##name
#define MONITOR_GET_REAL_NAME_HELP(var, name)  do {		\
    var = &name;						\
    MONITOR_DEBUG("%s() = %p\n", #name , var);			\
} while (0)
#define MONITOR_GET_REAL_NAME_WRAP_HELP(var, name)  do {	\
    var = &__real_##name;					\
    MONITOR_DEBUG("%s() = %p\n", "__real_" #name , var);	\
} while (0)
#else
#define MONITOR_WRAP_NAME_HELP(name)  name
#define MONITOR_GET_REAL_NAME_HELP(var, name)  \
    MONITOR_REQUIRE_DLSYM(var, #name )
#define MONITOR_GET_REAL_NAME_WRAP_HELP(var, name)  \
    MONITOR_REQUIRE_DLSYM(var, #name )
#endif

#define MONITOR_WRAP_NAME(name)  MONITOR_WRAP_NAME_HELP(name)
#define MONITOR_GET_REAL_NAME(var, name)  \
    MONITOR_GET_REAL_NAME_HELP(var, name)
#define MONITOR_GET_REAL_NAME_WRAP(var, name)  \
    MONITOR_GET_REAL_NAME_WRAP_HELP(var, name)

#define MONITOR_ASM_LABEL(name)		\
    asm volatile (".globl " #name );	\
    asm volatile ( #name ":" )

#define MONITOR_RUN_ONCE(var)				\
    static char monitor_has_run_##var = 0;		\
    if ( monitor_has_run_##var ) { return; }		\
    monitor_has_run_##var = 1

extern int monitor_debug;

void monitor_early_init(void);
void monitor_fork_init(void);
void monitor_signal_init(void);
void monitor_begin_process_fcn(void *, int);
void monitor_end_process_fcn(int);
void monitor_end_library_fcn(void);
void monitor_thread_shootdown(void);
int  monitor_shootdown_signal(void);
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
