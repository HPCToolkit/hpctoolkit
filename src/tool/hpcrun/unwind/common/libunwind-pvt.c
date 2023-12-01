// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2023, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

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
