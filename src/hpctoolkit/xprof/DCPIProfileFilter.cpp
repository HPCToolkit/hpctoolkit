// $Id$
// -*-C++-*-
// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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
//    DCPIFilterExpr.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include "PCProfileFilter.h"
#include "DCPIProfileFilter.h"
#include "DCPIProfileMetric.h"

//*************************** Forward Declarations ***************************

using std::endl;
using std::hex;
using std::dec;


//****************************************************************************

PCProfileFilter*
DCPIProfileFilter::RetiredInsn(LoadModule* lm)
{
  InsnFilter* i = new InsnFilter(InsnClassExpr(INSN_CLASS_ALL), lm);

  PCProfileFilter* f = new PCProfileFilter(PMMetric_Retired(), i);
  f->SetName("Retired_Insn");
  f->SetDescription("Retired Instructions");
  return f;
}

PCProfileFilter*
DCPIProfileFilter::RetiredFPInsn(LoadModule* lm)
{
  InsnFilter* i = new InsnFilter(InsnClassExpr(INSN_CLASS_FLOPS), lm);

  PCProfileFilter* f = new PCProfileFilter(PMMetric_Retired(), i);
  f->SetName("Retired_FP_Insn");
  f->SetDescription("Retired FP Instructions");
  return f;
}



DCPIMetricFilter* 
DCPIProfileFilter::PMMetric_Retired()
{
  DCPIMetricExpr e = DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count 
				    | DCPI_PM_ATTR_retired_T);
  DCPIMetricFilter* f = new DCPIMetricFilter(e);
  return f;
}

//****************************************************************************
// DCPIMetricFilter
//****************************************************************************

bool 
DCPIMetricFilter::operator()(const PCProfileMetric* m)
{
  const DCPIProfileMetric* dm = dynamic_cast<const DCPIProfileMetric*>(m);
  BriefAssertion(dm && "Internal Error: invalid cast!");
  
  const DCPIMetricDesc& mdesc = dm->GetDCPIDesc();
  if (mdesc.IsSet(expr)) {
    return true;
  } else {
    return false;
  }
}
