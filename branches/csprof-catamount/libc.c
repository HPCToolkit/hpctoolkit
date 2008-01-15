/* contains overrides for some critical functions in libc that we need
   to know about so that we can take appropriate action. */
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

#include <dlfcn.h>
#include <signal.h>
#include <sys/time.h>
#include <stdio.h>

#include "atomic.h"
#include "general.h"
#include "list.h"
#include "xpthread.h"
#include "interface.h"
#include "epoch.h"
#include "state.h"
#include "util.h"
#include "unsafe.h"
#include "libstubs.h"

#if 0
int (*csprof_sigaction)(int sig, struct sigaction *act,
                      struct sigaction *oact);
sig_t (*csprof_signal)(int sig, sig_t func);
#endif

#include <setjmp.h>

typedef void (*old_sig_handler_func_t)(int);

/* we are probably in trouble if this isn't defined, but... */
#ifndef RTLD_NEXT
#define RTLD_NEXT -1
#endif

/* static */ void (*csprof_longjmp)(jmp_buf, int);
/* static */ void (*csprof__longjmp)(jmp_buf, int);

static void (*csprof_siglongjmp)(jmp_buf, int);
/* static old_sig_handler_func_t (*csprof_signal)(int, old_sig_handler_func_t); */
static void *(*csprof_dlopen)(const char *, int);
#if defined(__osf__)
int (*csprof_setitimer)(int, const struct itimerval *, struct itimerval *);
#else
int (*csprof_setitimer)(int, const struct itimerval *, struct itimerval *);
#endif

int (*csprof_sigprocmask)(int, const sigset_t *, sigset_t *);
static void (*csprof_exit)(int);
static void (*csprof__exit)(int);

#if !defined(CSPROF_SYNCHRONOUS_PROFILING)
extern sigset_t prof_sigset;
#endif

static int library_stubs_initialized = 0;

static void
init_library_stubs()
{
/* we need the "csprof_" libc versions of certain functions, but we don't
   want to figure out if we've already located them each and every time
   we call the function.  do all of the necessary dlsym'ing here. */

    /* normal libc functions */
    CSPROF_GRAB_FUNCPTR(siglongjmp, siglongjmp);
    CSPROF_GRAB_FUNCPTR(longjmp, longjmp);
    CSPROF_GRAB_FUNCPTR(_longjmp, _longjmp);
    /* #ifdef NOMON */
    CSPROF_GRAB_FUNCPTR(dlopen, dlopen);
    /* #endif */
    CSPROF_GRAB_FUNCPTR(setitimer, setitimer);
    CSPROF_GRAB_FUNCPTR(sigprocmask, sigprocmask);
#ifdef NOMON
    CSPROF_GRAB_FUNCPTR(exit, exit);
    CSPROF_GRAB_FUNCPTR(_exit, _exit);
#endif

#if 0
    CSPROF_GRAB_FUNCPTR(sigaction, sigaction);
    CSPROF_GRAB_FUNCPTR(signal, signal);
    /*    real_sigaction = dlsym(RTLD_NEXT,"sigaction");
    real_signal = dlsym(RTLD_NEXT,"signal");
    */
#endif

    library_stubs_initialized = 1;
}

void
csprof_libc_init()
{
    MAYBE_INIT_STUBS();
    arch_libc_init();
}

#if defined(CSPROF_SYNCHRONOUS_PROFILING_ONLY)
#define NLX_GUTS
#else
#define NLX_GUTS \
    csprof_state_t *state; \
    sigset_t oldset; \
    MAYBE_INIT_STUBS(); \
    csprof_sigmask(SIG_BLOCK, &prof_sigset, &oldset); \
    state = csprof_get_state(); \
    /* move the trampoline and so forth appropriately */ \
    csprof_nlx_to_jmp_buf(state, env); \
    /* go */ \
    csprof_sigmask(SIG_SETMASK, &oldset, NULL);
#endif

/* libc override handling */

/* Only override these libc functions if we are using the trampoline */
#ifdef CSPROF_TRAMPOLINE_BACKEND

/* we need to know about this to unwind the backtrace properly.  we'd
   get extra bonus points if we could move the trampoline to underneath
   the function to which we're longjmp'ing. */
void
siglongjmp(sigjmp_buf env, int value)
{
    NLX_GUTS

    libcall2(csprof_siglongjmp, env, value);
}

void
longjmp(jmp_buf env, int value)
{
    NLX_GUTS

    csprof_longjmp(env, value);
}

void
_longjmp(jmp_buf env, int value)
{
    NLX_GUTS

    libcall2(csprof__longjmp, env, value);
}

#endif /* CSPROF_TRAMPOLINE_BACKEND defined */

#ifndef STATIC_ONLY
#if 0
sig_t signal(int sig, sig_t func) {
  EMSG("Override signal called");

  if (sig != SIGSEGV){
    EMSG("Override signal NON SEGV");
    return (*csprof_signal)(sig,func);
  }
  EMSG("ignored signal used to override SEGV");

  return 0;
}


int sigaction(int sig, const struct sigaction *act,
              struct sigaction *oact){

  static struct sigaction polite_act;
  extern void polite_segv_handler(int);
  extern void (*oth_segv_handler)(int);

  EMSG("Override sigaction called");
  if (sig != SIGSEGV){
    EMSG("Override NONsegv sigaction signal = %d called",sig);
    return (*csprof_sigaction)(sig,act,oact);
  }

  EMSG("Override SIGSEGV");
  memcpy(&polite_act,act,sizeof(struct sigaction));
  oth_segv_handler = act->sa_handler;
  polite_act.sa_sigaction = &polite_segv_handler;

  return (*csprof_sigaction)(sig,&polite_act,oact);
};
#endif
#endif

/* override setitimer() so that we're the only party setting the profiling
   signal.  this shouldn't cause any harm to the application.

   we have to use an old-school declaration (no `const') because of some
   preprocessor magic we unset, above (on Tru64, anyway); FIXME! */
#if 0
int
#if defined(__osf__)
setitimer(int which, const struct itimerval *value, struct itimerval *ovalue)
#else
    /* hate hate hate the glibc developers */
setitimer(__itimer_which_t which, const struct itimerval *value, struct itimerval *ovalue)
#endif
{
    MAYBE_INIT_STUBS();

    if(which == ITIMER_PROF) {
        /* success */
        return 0;
    }
    else {
        return libcall3(csprof_setitimer, which, value, ovalue);
    }
}
#endif
