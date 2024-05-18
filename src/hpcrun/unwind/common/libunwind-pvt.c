// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#define UNW_LOCAL_ONLY

#include "libunwind-pvt.h"

void libunwind_bind() {
}

int libunwind_getcontext(unw_context_t* uc) {
  return unw_getcontext(uc);
}

int libunwind_init_local(unw_cursor_t* cur, unw_context_t* uc) {
  return unw_init_local(cur, uc);
}

int libunwind_init_local2(unw_cursor_t* cur, unw_context_t* uc, int f) {
  return unw_init_local2(cur, uc, f);
}

int libunwind_step(unw_cursor_t* cur) {
  return unw_step(cur);
}

int libunwind_reg_states_iterate(unw_cursor_t* cur, unw_reg_states_callback cb, void* ud) {
  return unw_reg_states_iterate(cur, cb, ud);
}

int libunwind_apply_reg_state(unw_cursor_t* cur, void* st) {
  return unw_apply_reg_state(cur, st);
}

int libunwind_get_reg(unw_cursor_t* cur, int reg, unw_word_t* val) {
  return unw_get_reg(cur, reg, val);
}

int libunwind_set_reg(unw_cursor_t* cur, int reg, unw_word_t val) {
  return unw_set_reg(cur, reg, val);
}

int libunwind_get_save_loc(unw_cursor_t* cur, int x, unw_save_loc_t* y) {
  return unw_get_save_loc(cur, x, y);
}
