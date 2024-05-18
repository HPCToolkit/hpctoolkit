// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef LIBUNWIND_PVT_H
#define LIBUNWIND_PVT_H

#include <libunwind.h>

void libunwind_bind();

__attribute__((visibility("default")))
int libunwind_getcontext(unw_context_t*);

__attribute__((visibility("default")))
int libunwind_init_local(unw_cursor_t*, unw_context_t*);

__attribute__((visibility("default")))
int libunwind_init_local2(unw_cursor_t*, unw_context_t*, int);

__attribute__((visibility("default")))
int libunwind_step(unw_cursor_t*);

__attribute__((visibility("default")))
int libunwind_reg_states_iterate(unw_cursor_t*, unw_reg_states_callback, void*);

__attribute__((visibility("default")))
int libunwind_apply_reg_state(unw_cursor_t*, void*);

__attribute__((visibility("default")))
int libunwind_get_reg(unw_cursor_t*, int, unw_word_t*);

__attribute__((visibility("default")))
int libunwind_set_reg(unw_cursor_t*, int, unw_word_t);

__attribute__((visibility("default")))
int libunwind_get_save_loc(unw_cursor_t*, int, unw_save_loc_t*);

#endif  // LIBUNWIND_PVT_H
