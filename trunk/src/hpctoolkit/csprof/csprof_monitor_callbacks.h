#ifndef CSPROF_MONITOR_CALLBACK_H
#define CSPROF_MONITOR_CALLBACK_H

#include "killsafe.h"

extern void csprof_init_thread_support(void);
extern void *csprof_thread_pre_create(void);
extern void csprof_thread_post_create(void *dc);
extern void csprof_thread_init(killsafe_t *kk, int id, lush_cct_ctxt_t* thr_ctxt);
extern void csprof_thread_fini(csprof_state_t *state);
extern void csprof_init_internal(void);
extern void csprof_fini_internal(void);

#endif // ! defined(CSPROF_MONITOR_CALLBACK_H)
