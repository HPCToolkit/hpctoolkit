// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

// ----------------------------------------------------------------------
//
//   the implementation of evaluation expressions
//
// ----------------------------------------------------------------------

//************************ System Include Files ******************************

#include <iostream> 
using std::endl;

#include <sstream>
#include <string>
#include <algorithm>

#include <cmath>

//************************* User Include Files *******************************

#include <include/uint.h>

#include "Metric-AExprIncr.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/NaN.h>

//************************ Forward Declarations ******************************

#define AEXPR_DO_CHECK 0
#if (AEXPR_DO_CHECK)
# define AEXPR_CHECK(x) if (!isok(x)) { return c_FP_NAN_d; }
#else
# define AEXPR_CHECK(x) 
#endif

//static double epsilon = 0.000001;

//****************************************************************************

namespace Prof {

namespace Metric {


// ----------------------------------------------------------------------
// class AExprItrv
// ----------------------------------------------------------------------

std::string
AExprItrv::toString() const
{
  std::ostringstream os;
  dump(os);
  return os.str();
}


void
AExprItrv::ddump() const
{
  dump(std::cerr);
  std::cerr.flush();
}


// ----------------------------------------------------------------------
// class MinItrv
// ----------------------------------------------------------------------

std::ostream&
MinItrv::dumpMe(std::ostream& os) const
{
  os << "min()";
  return os;
}


// ----------------------------------------------------------------------
// class MaxItrv
// ----------------------------------------------------------------------

std::ostream&
MaxItrv::dumpMe(std::ostream& os) const
{
  os << "max()";
  return os;
}


// ----------------------------------------------------------------------
// class SumItrv
// ----------------------------------------------------------------------

std::ostream&
SumItrv::dumpMe(std::ostream& os) const
{
  os << "sum()";
  return os;
}


// ----------------------------------------------------------------------
// class MeanItrv
// ----------------------------------------------------------------------

std::ostream&
MeanItrv::dumpMe(std::ostream& os) const
{
  os << "mean()";
  return os;
}


// ----------------------------------------------------------------------
// class StdDevItrv
// ----------------------------------------------------------------------

std::ostream&
StdDevItrv::dumpMe(std::ostream& os) const
{
  os << "stddev()";
  return os;
}


// ----------------------------------------------------------------------
// class CoefVarItrv
// ----------------------------------------------------------------------

std::ostream&
CoefVarItrv::dumpMe(std::ostream& os) const
{
  os << "coefvar()";
  return os;
}


// ----------------------------------------------------------------------
// class RStdDevItrv
// ----------------------------------------------------------------------

std::ostream&
RStdDevItrv::dumpMe(std::ostream& os) const
{
  os << "r-stddev()";
  return os;
}

//****************************************************************************


} // namespace Metric

} // namespace Prof
