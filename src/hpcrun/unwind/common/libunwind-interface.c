// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "libunwind-interface.h"
#include "../hpcrun-sonames.h"

#include "../../audit/binding.h"

#include <stddef.h>

pfn_unw_getcontext_t libunwind_getcontext = NULL;
pfn_unw_init_local_t libunwind_init_local = NULL;
pfn_unw_init_local2_t libunwind_init_local2 = NULL;
pfn_unw_step_t libunwind_step = NULL;
pfn_unw_reg_states_iterate_t libunwind_reg_states_iterate = NULL;
pfn_unw_apply_reg_state_t libunwind_apply_reg_state = NULL;
pfn_unw_get_reg_t libunwind_get_reg = NULL;
pfn_set_reg_t libunwind_set_reg = NULL;
pfn_unw_get_save_loc_t libunwind_get_save_loc = NULL;

void libunwind_bind() {
  hpcrun_bind_private(HPCRUN_UNWIND_SO,
    "libunwind_getcontext", &libunwind_getcontext,
    "libunwind_init_local", &libunwind_init_local,
    "libunwind_init_local2", &libunwind_init_local2,
    "libunwind_step", &libunwind_step,
    "libunwind_reg_states_iterate", &libunwind_reg_states_iterate,
    "libunwind_apply_reg_state", &libunwind_apply_reg_state,
    "libunwind_get_reg", &libunwind_get_reg,
    "libunwind_set_reg", &libunwind_set_reg,
    "libunwind_get_save_loc", &libunwind_get_save_loc,
    NULL);
}
