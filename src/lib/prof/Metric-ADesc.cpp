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

//***************************************************************************
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
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>

#include <string>
using std::string;

#include <typeinfo>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "Metric-ADesc.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/StrUtil.hpp>

//*************************** Forward Declarations **************************


//***************************************************************************

namespace Prof {
namespace Metric {


//***************************************************************************
// ADesc
//***************************************************************************

const string ADesc::s_nameNULL = "";
const string ADesc::s_nameIncl = " (I)";
const string ADesc::s_nameExcl = " (E)";

const string ADesc::s_nameFmtTag = "{formatted}";


const std::string&
ADesc::ADescTyToString(ADescTy type)
{
  switch (type) {
    case TyNULL: return s_nameNULL;
    case TyIncl: return s_nameIncl;
    case TyExcl: return s_nameExcl;
    default: DIAG_Die(DIAG_Unimplemented);
  }
}


const char*
ADesc::ADescTyToXMLString(ADescTy type)
{
  switch (type) {
    case TyNULL: return "nil";
    case TyIncl: return "inclusive";
    case TyExcl: return "exclusive";
    default: DIAG_Die(DIAG_Unimplemented);
  }
}


ADesc::ADescTy
ADesc::stringToADescTy(const std::string& x)
{
  if (x == s_nameNULL) {
    return TyNULL;
  }
  else if (x == s_nameIncl) {
    return TyIncl;
  }
  else if (x == s_nameExcl) {
    return TyExcl;
  }
  else {
    DIAG_Die(DIAG_Unimplemented);
  }
}


std::string
ADesc::nameToFmt() const
{
  // acceptable to create on demand
  std::string nm;
  
  // {nameFmtTag}
  //   <prefix><base><suffix><type>
  //   <dbId><dbNumMetrics>

  nm += s_nameFmtTag;

  nm += s_nameFmtSegBeg + m_namePfx  + s_nameFmtSegEnd;
  nm += s_nameFmtSegBeg + m_nameBase + s_nameFmtSegEnd;
  nm += s_nameFmtSegBeg + m_nameSfx  + s_nameFmtSegEnd;
  nm += s_nameFmtSegBeg + ADescTyToString(type()) + s_nameFmtSegEnd;

  nm += s_nameFmtSegBeg + StrUtil::toStr(dbId()) + s_nameFmtSegEnd;
  nm += s_nameFmtSegBeg + StrUtil::toStr(dbNumMetrics()) + s_nameFmtSegEnd;
  
  return nm;
}


void
ADesc::nameFromString(const string& x)
{
  // {nameFmtTag}
  //   <prefix><base><suffix><type>
  //   <dbId><dbNumMetrics>

  if (x.compare(0, s_nameFmtTag.length(), s_nameFmtTag) == 0) {
    // parse formatted name
    size_t fmtBeg = s_nameFmtTag.length();
    for (uint i_seg = 1; i_seg <= 6; ++i_seg) {
      size_t segBeg = x.find_first_of(s_nameFmtSegBeg, fmtBeg);
      size_t segEnd = x.find_first_of(s_nameFmtSegEnd, fmtBeg);
      DIAG_Assert(segBeg == fmtBeg && segEnd != string::npos,
		  DIAG_UnexpectedInput << "'" << x << "'");
      segBeg++;
      string segStr = x.substr(segBeg, segEnd - segBeg);
      
      switch (i_seg) {
        case 1: namePfx(segStr);  break;
        case 2: nameBase(segStr); break;
        case 3: nameSfx(segStr);  break;
        case 4: type(stringToADescTy(segStr)); break;
        case 5: dbId((uint)StrUtil::toLong(segStr)); break;
        case 6: dbNumMetrics((uint)StrUtil::toLong(segStr)); break;
        default: DIAG_Die(DIAG_UnexpectedInput);
      }
      
      fmtBeg = segEnd + 1;
    }
  }
  else {
    nameBase(x);
  }
}


std::string
ADesc::toString() const
{
  string nm = name();
  if (0) {
    nm += " {dbId: " + StrUtil::toStr(dbId()) 
      + ", dbNumMetrics: " + StrUtil::toStr(dbNumMetrics()) + "}";
  }
  return nm;

  //std::ostringstream os;
  //dump(os);
  //return os.str();
}


std::ostream&
ADesc::dumpMe(std::ostream& os) const
{
  os << toString();
  return os;
}


void
ADesc::ddump() const
{
  dump(std::cerr);
  std::cerr.flush();
}

//***************************************************************************


MetricFlags_ValTy_t
ADesc::toHPCRunMetricValTy(ADesc::ADescTy ty)
{
  switch (ty) {
    case ADesc::TyNULL: return MetricFlags_ValTy_NULL;
    case ADesc::TyIncl: return MetricFlags_ValTy_Incl;
    case ADesc::TyExcl: return MetricFlags_ValTy_Excl;
    default:
      DIAG_Die(DIAG_UnexpectedInput);
  }
}


ADesc::ADescTy
ADesc::fromHPCRunMetricValTy(MetricFlags_ValTy_t ty)
{
  switch (ty) {
    case MetricFlags_ValTy_NULL: return ADesc::TyNULL;
    case MetricFlags_ValTy_Incl: return ADesc::TyIncl;
    case MetricFlags_ValTy_Excl: return ADesc::TyExcl;
    default:
      DIAG_Die(DIAG_UnexpectedInput);
  }
}



//***************************************************************************
// SampledDesc
//***************************************************************************

std::string
SampledDesc::toString() const
{
  string units = isUnitsEvents() ? " [events]" : " [samples]";
  string xtra = " {" + description() + ":" + StrUtil::toStr(period()) + " ev/smpl}";
  string str = ADesc::toString() + units + xtra;
  return str;
}


std::string
SampledDesc::toValueTyStringXML() const
{ 
  switch (computedType()) {
    case ComputedTy_NULL:
      return "raw";

    default:
    case ComputedTy_NonFinal:
      DIAG_Die(DIAG_Unimplemented);
      break;

    case ComputedTy_Final:
      DIAG_Assert(type() != TyNULL, "");
      return "final";
  }
  DIAG_Die(DIAG_Unimplemented);
}


std::ostream&
SampledDesc::dumpMe(std::ostream& os) const
{
  os << toString();
  return os;
}


//***************************************************************************
// DerivedDesc
//***************************************************************************

std::string
DerivedDesc::toString() const
{
  string exprStr = (m_expr) ? m_expr->toString() : "";
  string str = ADesc::toString() + " {" + exprStr + "}";
  return str;
}


std::string
DerivedDesc::toValueTyStringXML() const
{
  switch (computedType()) {
    default:
    case ComputedTy_NULL:
      DIAG_Die(DIAG_Unimplemented);
      break;

    case ComputedTy_NonFinal:
      return "derived-incr";

    case ComputedTy_Final:
      return "derived";
  }
  DIAG_Die(DIAG_Unimplemented);
}


std::ostream&
DerivedDesc::dumpMe(std::ostream& os) const
{
  os << toString();
  return os;
}


//***************************************************************************
// DerivedIncrDesc
//***************************************************************************

std::string
DerivedIncrDesc::toString() const
{
  string exprStr = (m_expr) ? m_expr->toString() : "";
  string str = ADesc::toString() + " {" + exprStr + "}";
  return str;
}


std::string
DerivedIncrDesc::toValueTyStringXML() const
{
  switch (computedType()) {
    default:
    case ComputedTy_NULL:
      DIAG_Die(DIAG_Unimplemented);
      break;

    case ComputedTy_NonFinal:
      return "derived-incr";
      
    case ComputedTy_Final:
      return "derived";
  }
  DIAG_Die(DIAG_Unimplemented);
}


std::ostream&
DerivedIncrDesc::dumpMe(std::ostream& os) const
{
  os << toString();
  return os;
}


//***************************************************************************

} // namespace Metric
} // namespace Prof

