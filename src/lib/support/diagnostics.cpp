// -*-Mode: C++;-*-

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
// Copyright ((c)) 2002-2016, Rice University
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

#include <include/gcc-attr.h>
#include <include/uint.h>


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


void
Diagnostics_TheMostVisitedBreakpointInHistory(const char* GCC_ATTR_UNUSED filenm,
					      uint GCC_ATTR_UNUSED lineno)
{
  // Prevent this routine from ever being inlined
  static uint count = 0;
  count++;
}


const char* DIAG_Unimplemented = 
  "Unimplemented feature: ";
const char* DIAG_UnexpectedInput = 
  "Unexpected input: ";
const char* DIAG_UnexpectedOpr = 
  "Unexpected operator: ";


//****************************************************************************
