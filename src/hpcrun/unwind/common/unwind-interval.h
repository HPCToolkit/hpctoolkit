// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef unwind_interval_h
#define unwind_interval_h

#include "binarytree_uwi.h"

//***************************************************************************
// external interface
//***************************************************************************

btuwi_status_t
build_intervals(char  *ins, unsigned int len, unwinder_t uw);

//***************************************************************************

#endif // unwind_interval_h
