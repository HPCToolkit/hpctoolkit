/* driver functions for sample-driven (asynchronous) profiling */
#ifndef CSPROF_DRIVER_H
#define CSPROF_DRIVER_H

#include "structs.h"

/* intended to be called at library initialization and finalization */
extern void csprof_driver_init(csprof_state_t *, csprof_options_t *);
extern void csprof_driver_fini(csprof_state_t *, csprof_options_t *);

#ifdef CSPROF_THREADS

/* intended to be called at thread initialization and finalization */
extern void csprof_driver_thread_init(csprof_state_t *);
extern void csprof_driver_thread_fini(csprof_state_t *);

#endif

/* called in the signal handler so spurious events don't happen.  these
   functions are called whenever the signal handler is entered/exited,
   even on multiple threads and should be prepared to handle this.

   FIXME: is this a good model beyond itimer/papi/pfmon? */
extern void csprof_driver_suspend(csprof_state_t *);
extern void csprof_driver_resume(csprof_state_t *);

/* called whenever code needs to be executed that cannot be profiled
   (generally because unwinding the stack from inside the code will
   result in serious problems) */
#if 1

/* FIXME: need CSPROF_{,A}SYNCHRONOUS_PROFILING */
#if defined(CSPROF_SYNCHRONOUS_PROFILING_ONLY)
typedef int csprof_profile_token_t;
#else
typedef sigset_t csprof_profile_token_t;
#endif

extern void csprof_driver_forbid_samples(csprof_state_t *,
                                         csprof_profile_token_t *);
extern void csprof_driver_enable_samples(csprof_state_t *,
                                         csprof_profile_token_t *);
#endif

/* sigset to block profiling signals */
extern sigset_t prof_sigset;

#endif
