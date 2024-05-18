// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   hpcrun metrics.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef prof_lean_hpcrun_metric_h
#define prof_lean_hpcrun_metric_h

//************************* System Include Files ****************************

#include <stdbool.h>

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif


//***************************************************************************
//
//***************************************************************************

// N.B. use a macro rather than a 'const char*' because the latter is
// treated as a constant variable and not an actual constant.
#define /*const char**/ HPCRUN_METRIC_RetCnt "RETCNT"


//***************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* prof_lean_hpcrun_metric_h */
