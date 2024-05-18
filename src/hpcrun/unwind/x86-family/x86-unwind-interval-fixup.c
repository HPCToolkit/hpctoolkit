// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//------------------------------------------------------------------------------
// system includes
//------------------------------------------------------------------------------
#define _GNU_SOURCE

#include <stdlib.h>



//------------------------------------------------------------------------------
// local includes
//------------------------------------------------------------------------------

#include "x86-unwind-interval-fixup.h"
#include "manual-intervals/x86-manual-intervals.h"



//------------------------------------------------------------------------------
// macros
//------------------------------------------------------------------------------

#define DECLARE_FIXUP_ROUTINE(fn) \
        extern int fn(char *ins, int len, btuwi_status_t* stat);

#define REGISTER_FIXUP_ROUTINE(fn) fn,



//------------------------------------------------------------------------------
// external declarations
//------------------------------------------------------------------------------

FORALL_X86_INTERVAL_FIXUP_ROUTINES(DECLARE_FIXUP_ROUTINE)



//------------------------------------------------------------------------------
// local variables
//------------------------------------------------------------------------------

static x86_ui_fixup_fn_t x86_interval_fixup_routine_vector[] = {
  FORALL_X86_INTERVAL_FIXUP_ROUTINES(REGISTER_FIXUP_ROUTINE)
  0
};



//------------------------------------------------------------------------------
// interface operations
//------------------------------------------------------------------------------

int
x86_fix_unwind_intervals(char *ins, int len, btuwi_status_t *stat)
{
   x86_ui_fixup_fn_t *fn = &x86_interval_fixup_routine_vector[0];

   while (*fn) {
    if ((*fn++)(ins,len,stat)) return 1;
   }
   return 0;
}
