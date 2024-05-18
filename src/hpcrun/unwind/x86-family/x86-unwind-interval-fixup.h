// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef x96_unwind_interval_fixup_h
#define x96_unwind_interval_fixup_h

//******************************************************************************
// local includes
//******************************************************************************

#include "x86-unwind-interval.h"



//******************************************************************************
// types
//******************************************************************************

typedef int (*x86_ui_fixup_fn_t)(char *ins, int len, btuwi_status_t *stat);



//******************************************************************************
// forward declarations
//******************************************************************************

int x86_fix_unwind_intervals(char *ins, int len, btuwi_status_t *stat);

#endif
