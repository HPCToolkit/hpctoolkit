// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef x86_unwind_analysis_h
#define x86_unwind_analysis_h

//************************** XED Include Files ******************************

#if __has_include(<xed/xed-interface.h>)
#include <xed/xed-interface.h>
#else
#include <xed-interface.h>
#endif

//*************************** User Include Files ****************************

#include "x86-unwind-interval.h"

#include "../../memory/hpcrun-malloc.h"

//***************************************************************************

extern void *x86_get_branch_target(void *ins,xed_decoded_inst_t *xptr);

#define FIX_INTERVALS_AT_RETURN

#endif  // x86_unwind_analysis_h
