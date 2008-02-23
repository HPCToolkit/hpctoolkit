#ifndef CSPROF_MONITOR_CALLBACK_H
#define CSPROF_MONITOR_CALLBACK_H

#include "killsafe.h"

void csprof_init_thread_support(int id);

void *csprof_thread_pre_create(void);

void csprof_thread_post_create(void *dc);

void csprof_thread_init(killsafe_t *kk, int id, lush_cct_ctxt_t* thr_ctxt);

void csprof_thread_fini(csprof_state_t *state);

void csprof_init_internal(void);

void csprof_fini_internal(void);

#endif // ! defined(CSPROF_MONITOR_CALLBACK_H)
