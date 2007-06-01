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

int (*real_sigaction)(int sig, struct sigaction *act,
                      struct sigaction *oact);
sig_t (*real_signal)(int sig, sig_t func);

/***** MWF set brackets for backtrace ****/
void backtrace_set_brackets(void (*a)(void),void (*b)(void),
                            void *__unbounded s);

/***** MWF added grab start_main for fenceposting stack ****/
#define PARAMS_START_MAIN (int (*main) (int, char **, char **),              \
			   int argc,                                         \
			   char *__unbounded *__unbounded ubp_av,            \
			   void (*init) (void),                              \
                           void (*fini) (void),                              \
			   void (*rtld_fini) (void),                         \
			   void *__unbounded stack_end)

typedef int (*libc_start_main_fptr_t) PARAMS_START_MAIN;
typedef void (*libc_start_main_fini_fptr_t) (void);

/* libc intercepted routines */
static int  csprof_libc_start_main PARAMS_START_MAIN;
/* static void hpcrun_libc_start_main_fini(); */

/* Intercepted libc routines: set when the library is
   initialized */
static libc_start_main_fptr_t      real_start_main;

#include <setjmp.h>

typedef void (*old_sig_handler_func_t)(int);

/* FIXME: grotty FIXED_LIBCALLS stuff */

/* we are probably in trouble if this isn't defined, but... */
#ifndef RTLD_NEXT
#define RTLD_NEXT -1
#endif

/* libc functions which we need to override */
#ifdef CSPROF_FIXED_LIBCALLS
/* these are hacks and need to be changed depending on the libc version */
static void (*csprof_longjmp)(jmp_buf, int)
    = 0x3ff800f8550;
static void (*csprof_siglongjmp)(jmp_buf, int)
    = 0x3ff801b2a40;
static void (*csprof__longjmp)(jmp_buf, int)
    = 0x3ff801b2d40;
static old_sig_handler_func_t (*csprof_signal)(int, old_sig_handler_func_t)
    = 0x3ff800e0b50;
static void *(*csprof_dlopen)(const char *, int)
    = 0x3ff800e2bb0;
int (*csprof_setitimer)(int, struct itimerval *, struct itimerval *)
    = 0x3ff800d55f0;
int (*csprof_sigprocmask)(int, const sigset_t *, sigset_t *)
    = 0x3ff800d7d30;
static void (*csprof_exit)(int)
    = 0x3ff800dde90;
#else
    /* static */ void (*csprof_longjmp)(jmp_buf, int);
    /* static */ void (*csprof__longjmp)(jmp_buf, int);
static void (*csprof_siglongjmp)(jmp_buf, int);
static old_sig_handler_func_t (*csprof_signal)(int, old_sig_handler_func_t);
static void *(*csprof_dlopen)(const char *, int);
#if defined(__osf__)
int (*csprof_setitimer)(int, const struct itimerval *, struct itimerval *);
#else
int (*csprof_setitimer)(int, const struct itimerval *, struct itimerval *);
#endif

int (*csprof_sigprocmask)(int, const sigset_t *, sigset_t *);
static void (*csprof_exit)(int);
static void (*csprof__exit)(int);
#endif

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
#ifndef CSPROF_FIXED_LIBCALLS
    /* specious generality which might be needed later */

    /**** MWF grab libc_start_main, lifted from hpcrun code ****/
    fprintf(stderr,"attempting go grab libc_start_main\n");
    real_start_main = 
    (libc_start_main_fptr_t)dlsym(RTLD_NEXT, "__libc_start_main");
    if (!real_start_main) {
      real_start_main = 
        (libc_start_main_fptr_t)dlsym(RTLD_NEXT, "__BP___libc_start_main");
    }
    if (!real_start_main) {
      fprintf(stderr,"!! Failed to grab libc_start_main !!\n");
    }
  /*** MWF END of grab libc_start_main ***/


    /* normal libc functions */
    CSPROF_GRAB_FUNCPTR(siglongjmp, siglongjmp);
    CSPROF_GRAB_FUNCPTR(longjmp, longjmp);
    CSPROF_GRAB_FUNCPTR(_longjmp, _longjmp);
    CSPROF_GRAB_FUNCPTR(signal, signal);
    CSPROF_GRAB_FUNCPTR(dlopen, dlopen);
    CSPROF_GRAB_FUNCPTR(setitimer, setitimer);
    CSPROF_GRAB_FUNCPTR(sigprocmask, sigprocmask);
    CSPROF_GRAB_FUNCPTR(exit, exit);
    CSPROF_GRAB_FUNCPTR(_exit, _exit);

    real_sigaction = dlsym(RTLD_NEXT,"sigaction");
    real_signal = dlsym(RTLD_NEXT,"signal");

    library_stubs_initialized = 1;
#endif
}

void
csprof_libc_init()
{
    MAYBE_INIT_STUBS();
    arch_libc_init();
}

/*
 *** MWF grabbed from hpcrun ***
 *  Intercept the start routine: this is from glibc and can be one of
 *  two different names depending on how the macro BP_SYM is defined.
 *    glibc-x/sysdeps/generic/libc-start.c
 */

/** MWF NOTE the _gnxl0, _gnxl1 null fencepost routines here **/

extern void _gnxl0(void){int _dc; _dc = 0;}
extern int 
__libc_start_main PARAMS_START_MAIN
{
  csprof_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}
extern void _gnxl1(void){int _dc; _dc = 1;}

extern int 
__BP___libc_start_main PARAMS_START_MAIN
{
  csprof_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}

static int (*the_main)(int, char **, char **);

static int faux_main(int n, char **argv, char **env){
  MSG(1,"calling regular main f faux main");
  return (*the_main)(n,argv,env);
}

static int 
csprof_libc_start_main PARAMS_START_MAIN
{
  /* launch the process */
  fprintf(stderr, "intercepted start main");
  fprintf(stderr,"_libc_start_main arguments: main: %p, argc: %d, upb_ac: %p,"
     "init: %p, fini: %p, rtld: %p, stack_end: %p\n",
     main, argc, ubp_av, init, fini, rtld_fini, stack_end);

  fprintf(stderr,"**rb = %lx, **lb = %lx, libc_sm = %lx\n",
          &_gnxl0,&_gnxl1,&__libc_start_main);
  backtrace_set_brackets(&_gnxl0,&_gnxl1,stack_end);
  MSG(1,"using faux_main now");
  the_main = main;
  (*real_start_main)(faux_main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
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

#if 0
/* somehow we need to expose this to the main library */
static old_sig_handler_func_t oldprof_func;

/* attempt to stop uninformed people from yanking out our profiler */
old_sig_handler_func_t
signal(int sig, old_sig_handler_func_t func)
{
    MAYBE_INIT_STUBS();

    if(sig != SIGPROF) {
        return libcall2(csprof_signal, sig, func);
    }
    else {
        return func;
    }
}
#endif

sig_t signal(int sig, sig_t func) {
  EMSG("Override signal called");

  if (sig != SIGSEGV){
    EMSG("Override signal NON SEGV");
    return (*real_signal)(sig,func);
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
    return (*real_sigaction)(sig,act,oact);
  }

  EMSG("Override SIGSEGV");
  memcpy(&polite_act,act,sizeof(struct sigaction));
  oth_segv_handler = act->sa_handler;
  polite_act.sa_sigaction = &polite_segv_handler;

  return (*real_sigaction)(sig,&polite_act,oact);
};

/* this override appears to be problematic */
#if 0
int
sigprocmask(int mode, const sigset_t *inset, sigset_t *outset)
{
    /* FIXME: duplication between pthread_sigmask and this function.
       Should have a `csprof_sanitize_sigset' somewhere. */
    sigset_t safe_inset;

    MAYBE_INIT_STUBS();

    if(inset != NULL) {
        /* sanitize */
        memcpy(&safe_inset, inset, sizeof(sigset_t));

        if(sigismember(&safe_inset, SIGPROF)) {
            sigdelset(&safe_inset, SIGPROF);
        }
    }

    return libcall3(csprof_sigprocmask, mode, &safe_inset, outset);
}
#endif

/* override setitimer() so that we're the only party setting the profiling
   signal.  this shouldn't cause any harm to the application.

   we have to use an old-school declaration (no `const') because of some
   preprocessor magic we unset, above (on Tru64, anyway); FIXME! */
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

void
exit(int status)
{
    MAYBE_INIT_STUBS();

    MSG(CSPROF_MSG_SHUTDOWN, "Exiting...");

    libcall1(csprof_exit, status);
}

void
_exit(int status)
{
    MAYBE_INIT_STUBS();

    MSG(CSPROF_MSG_SHUTDOWN, "Exiting via the _exit call");

#ifdef CSPROF_THREADS
    MSG(CSPROF_MSG_SHUTDOWN, "Thread %ld calling...", pthread_self());
#endif

    libcall1(csprof__exit, status);
}

/* going to have to rearrange load maps and things */
void *
dlopen(const char *file, int mode)
{
    csprof_epoch_t *epoch;
    void *ret;

    /* locking should not be needed during this function.  why?

       1. the C library should already provide some sort of
          synchronization between different threads calling dlopen().

       2. it follows that weird results should not be observed in
          the epoch code which records the load modules visible at
          the current time.

       3. functions in a particular shared library cannot be called
          until the dlopen which loads said shared library has
          returned.  thus, if we receive signals during the dlopen
          of a library, we can be assured that there will not be
          wacky addresses floating around on our backtrace.

       if we don't provide locking, we may need to somehow force
       csprof_get_epoch() to always use the most current epoch,
       which has the potential to be quite slow (memory barrier
       instructions on every access, perhaps?).  and csprof_get_epoch
       is called far more often than dlopen.  so perhaps doing
       appropriate locking here would be a win. */

    csprof_epoch_lock();

    MAYBE_INIT_STUBS();

    ret = libcall2(csprof_dlopen, file, mode);

    /* create a new epoch */
    epoch = csprof_epoch_new();

    csprof_epoch_unlock();

    return ret;
}
