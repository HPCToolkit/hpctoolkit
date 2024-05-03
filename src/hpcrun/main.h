// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef main_h
#define main_h

#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

extern bool hpcrun_is_initialized();

extern bool hpcrun_is_safe_to_sync(const char* fn);
extern void hpcrun_set_safe_to_sync(void);

extern void hpcrun_force_dlopen(bool forced);

//
// fetch the full path of the execname
//
extern char* hpcrun_get_execname(void);

typedef void siglongjmp_fcn(sigjmp_buf, int);

typedef struct hpcrun_aux_cleanup_t  hpcrun_aux_cleanup_t;

hpcrun_aux_cleanup_t * hpcrun_process_aux_cleanup_add( void (*func) (void *), void * arg);
void hpcrun_process_aux_cleanup_remove(hpcrun_aux_cleanup_t * node);

// ** HACK to accommodate PAPI-C w cuda component & gpu blame shifting

extern void special_cuda_ctxt_actions(bool enable);

extern bool hpcrun_suppress_sample();

extern void hpcrun_prepare_measurement_subsystem(bool is_child);

#endif  // ! main_h
