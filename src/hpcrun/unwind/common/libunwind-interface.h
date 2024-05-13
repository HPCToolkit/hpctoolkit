// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef LIBUNWIND_INTERFACE_H
#define LIBUNWIND_INTERFACE_H

#include <libunwind.h>

void libunwind_bind();

typedef int (*pfn_unw_getcontext_t)(unw_context_t*);
extern pfn_unw_getcontext_t libunwind_getcontext;

typedef int (*pfn_unw_init_local_t)(unw_cursor_t*, unw_context_t*);
extern pfn_unw_init_local_t libunwind_init_local;

typedef int (*pfn_unw_init_local2_t)(unw_cursor_t*, unw_context_t*, int);
extern pfn_unw_init_local2_t libunwind_init_local2;

typedef int (*pfn_unw_step_t)(unw_cursor_t*);
extern pfn_unw_step_t libunwind_step;

typedef int (*pfn_unw_reg_states_iterate_t)(unw_cursor_t*, unw_reg_states_callback, void*);
extern pfn_unw_reg_states_iterate_t libunwind_reg_states_iterate;

typedef int (*pfn_unw_apply_reg_state_t)(unw_cursor_t*, void*);
extern pfn_unw_apply_reg_state_t libunwind_apply_reg_state;

typedef int (*pfn_unw_get_reg_t)(unw_cursor_t*, int, unw_word_t*);
extern pfn_unw_get_reg_t libunwind_get_reg;

typedef int (*pfn_set_reg_t)(unw_cursor_t*, int, unw_word_t);
extern pfn_set_reg_t libunwind_set_reg;

typedef int (*pfn_unw_get_save_loc_t)(unw_cursor_t*, int, unw_save_loc_t*);
extern pfn_unw_get_save_loc_t libunwind_get_save_loc;

#endif  // LIBUNWIND_INTERFACE_H
