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

//  nan := 0.0/0.0
// +inf := 1.0/0.0
// -inf := -1.0/0.0

// ANSI/IEEE Standard 754-1985 (64-bit double precision)
//
//   S (1 bit)   E (11 bits)   F (52 bits)
//
//   * If E=2047 and F is nonzero, then V=NaN ("Not a number")
//   * If E=2047 and F is zero and S is 1, then V=-Infinity
//   * If E=2047 and F is zero and S is 0, then V=Infinity

// ANSI/IEEE Standard 754-1985 (128-bit quadruple precision)
//
//   S (1 bit)   E (15 bits)   F (112 bits)

//************************ System Include Files ******************************

#include <math.h> // C99: FP_NAN, isnan, isinf

//*************************** User Include Files ****************************

#include "NaN.h"

//*************************** Forward Declarations ***************************

const double c_FP_NAN_d = FP_NAN;

//****************************************************************************

bool
c_isnan_d(double x)
{
  return isnan(x);
}


bool
c_isinf_d(double x)
{
  return isinf(x);
}
