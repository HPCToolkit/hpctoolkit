// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
