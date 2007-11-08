/* This code has been stripped out and modified slightly by Philip Mucci,
University of Tennessee. This is OPEN SOURCE. */

/* -*-Mode: C;-*- */
/* $Id: monitor_preload.h,v 1.3 2007/05/24 22:54:36 krentel Exp krentel $ */

/****************************************************************************
//
// File:
//    $Source: /home/krentel/monitor/src/RCS/monitor_preload.h,v $
//
// Purpose:
//    General header.
//
// Description:
//    Shared declarations, etc for monitoring.
//    
//    *** N.B. *** 
//    There is an automake install hook that will hide these external
//    symbols.
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
*****************************************************************************/

#ifndef _MONITOR_PRELOAD_H
#define _MONITOR_PRELOAD_H

/************************** System Include Files ****************************/

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>      /* getpid() */
#include <inttypes.h>
#include <memory.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <syscall.h>

#ifndef __USE_GNU
# define __USE_GNU /* must define on Linux to get RTLD_NEXT from <dlfcn.h> */
# define SELF_DEFINED__USE_GNU
#endif

#include <dlfcn.h>

#undef __USE_GNU

/**************************** Forward Declarations **************************/

/* 'intercepted' libc routines */

#define PARAMS_START_MAIN (int (*main) (int, char **, char **),              \
			   int argc,                                         \
			   char *__unbounded *__unbounded ubp_av,            \
			   void (*init) (void),                              \
                           void (*fini) (void),                              \
			   void (*rtld_fini) (void),                         \
			   void *__unbounded stack_end)

typedef int (*libc_start_main_fptr_t) PARAMS_START_MAIN;
typedef void (*libc_start_main_fini_fptr_t) (void);

#define PARAMS_EXECV  (const char *path, char *const argv[])
#define PARAMS_EXECVP (const char *file, char *const argv[])
#define PARAMS_EXECVE (const char *path, char *const argv[],                 \
                       char *const envp[])
#define PARAMS_DLOPEN (const char *filename, int flag)
#define PARAMS_PTHREAD_CREATE (pthread_t* thread,                            \
			       const pthread_attr_t* attr,                   \
			       void *(*start_routine)(void*),                \
			       void* arg)

typedef void * (*dlopen_fptr_t) PARAMS_DLOPEN;
typedef int (*execv_fptr_t)  PARAMS_EXECV;
typedef int (*execvp_fptr_t) PARAMS_EXECVP;
typedef int (*execve_fptr_t) PARAMS_EXECVE;
typedef int (*pthread_create_fptr_t) PARAMS_PTHREAD_CREATE;
typedef pthread_t (*pthread_self_fptr_t) (void);
typedef void (*pthread_exit_fptr_t) (void *);
typedef pid_t (*fork_fptr_t) (void);
typedef void (*_exit_fptr_t) (int) __attribute__ ((noreturn));
typedef void (*exit_fptr_t) (int) __attribute__ ((noreturn));

#endif
