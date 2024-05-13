// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

//****************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent
//
//****************************************************************************

//************************** System Include Files ****************************

//************************** Open64 Include Files ***************************

//*************************** User Include Files *****************************

#include "lean/gcc-attr.h"


#include "diagnostics.h"

//****************************************************************************

int DIAG_DBG_LVL_PUB = 0;

void
Diagnostics_SetDiagnosticFilterLevel(int lvl)
{
  DIAG_DBG_LVL_PUB = lvl;
}


int
Diagnostics_GetDiagnosticFilterLevel()
{
  return DIAG_DBG_LVL_PUB;
}


static unsigned int count = 0;
void
Diagnostics_TheMostVisitedBreakpointInHistory(const char* GCC_ATTR_UNUSED filenm,
                                              unsigned int GCC_ATTR_UNUSED lineno)
{
  // Prevent this routine from ever being inlined
  count++;
}


const char* DIAG_Unimplemented =
  "Unimplemented feature: ";
const char* DIAG_UnexpectedInput =
  "Unexpected input: ";
const char* DIAG_UnexpectedOpr =
  "Unexpected operator: ";


//****************************************************************************
