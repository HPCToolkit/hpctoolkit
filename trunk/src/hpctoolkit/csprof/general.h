// $Id$
// -*-C-*-
// * BeginRiceCopyright *****************************************************
/*
  Copyright ((c)) 2002, Rice University 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  * Neither the name of Rice University (RICE) nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  This software is provided by RICE and contributors "as is" and any
  express or implied warranties, including, but not limited to, the
  implied warranties of merchantability and fitness for a particular
  purpose are disclaimed. In no event shall RICE or contributors be
  liable for any direct, indirect, incidental, special, exemplary, or
  consequential damages (including, but not limited to, procurement of
  substitute goods or services; loss of use, data, or profits; or
  business interruption) however caused and on any theory of liability,
  whether in contract, strict liability, or tort (including negligence
  or otherwise) arising in any way out of the use of this software, even
  if advised of the possibility of such damage.
*/
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//    general.h
//
// Purpose:
//    Useful definitions, etc. for use throughout the profiler.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef CSPROFLIB_GENERAL_H
#define CSPROFLIB_GENERAL_H

#ifdef CSPROF_THREADS
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#if defined(__cplusplus)
extern "C" {
#endif

    /* debugging messages which users might find entertaining */
#if defined(CSPROF_PERF)
#define CSPROF_DBG_LVL_PUB 0
#else
extern int CSPROF_DBG_LVL_PUB; // default: 0
#endif

    /* debugging levels */
#define CSPROF_DBG_STACK_TRAMPOLINE (1 << 1)
#define CSPROF_DBG_UNWINDING (1 << 2)
#define CSPROF_DBG_CCT_INSERTION (1 << 3)
#define CSPROF_DBG_UNW_TAILCALL (1 << 4)
#define CSPROF_DBG_UNW_SPRET (1 << 5)
#define CSPROF_DBG_REGISTER_TRAMPOLINE (1 << 6)
#define CSPROF_DBG_MALLOCPROF (1 << 7)
#define CSPROF_DBG_PTHREAD (1 << 8)

    /* messages for public consumption */
#define CSPROF_MSG_MEMORY (1 << 1)
#define CSPROF_MSG_EPOCH (1 << 2)
#define CSPROF_MSG_SHUTDOWN (1 << 3)
#define CSPROF_MSG_DATAFILE (1 << 4)

#if defined(CSPROF_PERF)
#define CSPROF_MSG_LVL 0
#else
extern int CSPROF_MSG_LVL; // default 0
#endif

/* Common function status values */
#define CSPROF_TRAMPOLINE 2     /* hack */
#define CSPROF_OK      1
#define CSPROF_ERR    -1

#define PRINT(buf) fwrite_unlocked(buf,1,n,stderr);
#define PSTR(str) do { \
size_t i = strlen(str); \
fwrite_unlocked(str, 1, i, stderr); \
} while(0)

/* pervasive use of a character buffer because the direct
   printf-to-a-stream functions seem to have problems */

static inline void MSG(int level, char *format, ...)
#if defined(CSPROF_PERF)
{
}
#else
{
    va_list args;
    char buf[512];
    va_start(args, format);
    if (level & CSPROF_MSG_LVL) {
        int n;
        flockfile(stderr);
#ifdef CSPROF_THREADS
	n = sprintf(buf, "csprof msg [%d][%lx]: ", level, pthread_self());
#else
        n = sprintf(buf, "csprof msg [%d]: ", level);
#endif
        PRINT(buf);
        n = vsprintf(buf, format, args);
        PRINT(buf);
        PSTR("\n");
        fflush_unlocked(stderr);
        funlockfile(stderr);
    }
    va_end(args);
}
#endif

static inline void DBGMSG_PUB(int level, char *format, ...)
#if defined(CSPROF_PERF)
{
}
#else
{
    va_list args;
    char buf[512];
    va_start(args, format);
    if (level & CSPROF_DBG_LVL_PUB) {
        int n;
        flockfile(stderr);
#ifdef CSPROF_THREADS
        n = sprintf(buf, "[%d][%lx]: ", level, pthread_self());
#else
        n = sprintf(buf, "[%d]: ", level);
#endif
        PRINT(buf);
        n = vsprintf(buf, format, args);
        PRINT(buf);
        PSTR("\n");
        fflush_unlocked(stderr);
        funlockfile(stderr);
    }
    va_end(args);
}
#endif

static inline void ERRMSGa(char *format, char *file, int line, va_list args) 
{ 
    char buf[512];
    int n;
    flockfile(stderr);
    n = sprintf(buf, "csprof error [%s:%d]:", file, line);
    PRINT(buf);
    n = vsprintf(buf, format, args);
    PRINT(buf);
    PSTR("\n");
    fflush_unlocked(stderr);
    funlockfile(stderr);
}

static inline void ERRMSG(char *format, char *file, int line, ...) 
#if 0
{
}
#else
{
    va_list args;
    va_start(args, line);
    ERRMSGa(format, file, line, args);
    va_end(args);
}
#endif

static inline void DIE(char *format, char *file, int line, ...) 
{
    va_list args;
    va_start(args, line);
    ERRMSGa(format, file, line, args);
    va_end(args);
    abort();
}

#define IFMSG(level) if (level & CSPROF_MSG_LVL)

#define IFDBG(level) if (level & CSPROF_DBG_LVL)

#define IFDBG_PUB(level) if (level & CSPROF_DBG_LVL_PUB)


/* mostly to shut the compiler up.  Digital's compiler automagically
   converts arithmetic on void pointers to arithmetic on character
   pointers, but it complains while doing so.  messages are so tiresome,
   plus it's not guaranteed that other compilers will implement the
   same semantics; this macro ensures that the semantics are the same
   everywhere. */
#define VPTR_ADD(vptr, amount) ((void *)(((char *)(vptr)) + (amount)))

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif
