// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
// This software was produced with support in part from the Defense Advanced
// Research Projects Agency (DARPA) through AFRL Contract FA8650-09-C-1915.
// Nothing in this work should be construed as reflecting the official policy or
// position of the Defense Department, the United States government, or
// Rice University.
//
//***************************************************************************


#ifndef __LIBUNW_INTERVALS_H__
#define __LIBUNW_INTERVALS_H__

#include "binarytree_uwi.h"
#include "std_unw_cursor.h"
#include "unwind.h"

void libunw_unw_init_cursor(hpcrun_unw_cursor_t* cursor, void* context);

btuwi_status_t libunw_build_intervals(char *beg_insn, unsigned int len);

bool libunw_finalize_cursor(hpcrun_unw_cursor_t* cursor, int decrement_pc);

step_state libunw_take_step(hpcrun_unw_cursor_t* cursor);

step_state libunw_unw_step(hpcrun_unw_cursor_t* cursor);

void libunw_uw_recipe_tostr(void* uwr, char str[]);

#endif
