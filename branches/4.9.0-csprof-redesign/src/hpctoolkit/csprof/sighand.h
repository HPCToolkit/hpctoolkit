#ifndef CSPROF_SIGHAND_H
#define CSPROF_SIGHAND_H

#include "state.h"

extern int status;

/* signal handler guts */
/* KLUDGE BLECH ARGH XXX */
#if defined(CSPROF_THREADS) && defined(CSPROF_ROUND_ROBIN_SIGNAL)
#include "atomic.h"
#define CSPROF_SIGNAL_HANDLER_GUTS(context_variable) \
do { \
  struct ucontext *ctx = (struct ucontext *)(context_variable); \
  if(status != CSPROF_STATUS_FINI) { \
    csprof_state_t *state = csprof_get_state(); \
    csprof_driver_suspend(state); \
    csprof_driver_distribute_samples(state, ctx); \
    csprof_driver_resume(state); \
  } \
} while(0)
#else
#define CSPROF_SIGNAL_HANDLER_GUTS(context_variable) \
do { \
    struct ucontext *ctx = (struct ucontext *)(context_variable); \
    if(status != CSPROF_STATUS_FINI) { \
        csprof_state_t *state = csprof_get_state(); \
        csprof_driver_suspend(state); \
        if(state != NULL) { \
            csprof_take_profile_sample(state, ctx); \
            csprof_state_flag_clear(state, CSPROF_THRU_TRAMP); \
        } \
        csprof_driver_resume(state); \
    } \
} while(0)
#endif

#endif
