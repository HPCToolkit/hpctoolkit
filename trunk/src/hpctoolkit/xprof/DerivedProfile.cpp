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
//    DerivedProfile.C
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

#include "DerivedProfile.h"

//*************************** Forward Declarations ***************************

using std::endl;
using std::hex;
using std::dec;

static bool 
VerifyPeriod(const PCProfileMetricSet* s);
static ulong 
GetPeriod(const PCProfileMetricSet* s);
String
GetNativeName(const PCProfileMetricSet* s);

//****************************************************************************
// DerivedProfile
//****************************************************************************

DerivedProfile::DerivedProfile()
  : metricVec(0)
{
}

DerivedProfile::DerivedProfile(const PCProfile* pcprof_, 
			       const PCProfileFilterList* filtlist)
  : metricVec(0)
{
  Create(pcprof_, filtlist);
}

DerivedProfile::~DerivedProfile()
{
  for (DerivedProfile_MetricIterator it(*this); it.IsValid(); ++it) {
    DerivedProfileMetric* dm = it.Current();
    delete dm;
  }
  metricVec.clear(); 
}

void
DerivedProfile::Create(const PCProfile* pcprof_, 
		       const PCProfileFilterList* filtlist)
{
  pcprof = pcprof_;

  // -------------------------------------------------------
  // 1. Create metrics.  As a special case, if filtlist is null we
  // have a one to one mapping between the raw and derived profile
  // metrics (identity filter).
  // -------------------------------------------------------
  if (filtlist == NULL) {
    SetNumMetrics(pcprof->GetNumMetrics());
    for (suint i = 0; i < pcprof->GetNumMetrics(); ++i) {
      const PCProfileMetric* m = pcprof->GetMetric(i);
      DerivedProfileMetric* dm = new DerivedProfileMetric(m);
      SetMetric(i, dm);
    }
  } else {
    // For each metric filter, create a derived metric
    SetNumMetrics(filtlist->size());
    PCProfileFilterListCIt it = filtlist->begin();
    for (suint i = 0; it != filtlist->end(); ++it, ++i) {
      PCProfileFilter* filt = *it;
      MetricFilter* mfilt = filt->GetMetricFilter();
      const PCProfileMetricSet* s = pcprof->Filter(mfilt);
      BriefAssertion(VerifyPeriod(s)); // we should all have the same period
      
      DerivedProfileMetric* dm = new DerivedProfileMetric(s);
      dm->SetName(filt->GetName());
      dm->SetNativeName(GetNativeName(s));
      dm->SetDescription(filt->GetDescription());
      dm->SetPeriod(GetPeriod(s));
      SetMetric(i, dm);
    }
  }
  
  // -------------------------------------------------------
  // for each pc
  //   for each derived metric and its insn filter
  //     if insn filter is satisfied for pc && pc has data
  //        record this pc in prof
  // as a side effect we could build a pc2srcline map if one is not available
  // -------------------------------------------------------
  // skip this for now... 
}

void 
DerivedProfile::Dump(std::ostream& o)
{
}

void 
DerivedProfile::DDump()
{
  Dump(std::cerr);
}

//****************************************************************************
// DerivedProfileMetric
//****************************************************************************

DerivedProfileMetric::DerivedProfileMetric(const PCProfileMetric* m)
{
  PCProfileMetricSet* s = new PCProfileMetricSet(1);
  s->Add(m);
  SetMetricSet(s);
  SetName(m->GetName());
  SetNativeName(m->GetName());
  SetDescription(m->GetDescription());
  SetPeriod(m->GetPeriod());
}

DerivedProfileMetric::~DerivedProfileMetric()
{
  // we own the set container, but not the contents!
  set->Clear(); 
  delete set;
}

void 
DerivedProfileMetric::Dump(std::ostream& o)
{
  o << "'DerivedProfileMetric' --\n";
  o << "  name: " << name << "\n";
  o << "  description: " << description << "\n";
  o << "  period: " << period << "\n";
}

void
DerivedProfileMetric::DDump()
{
  Dump(std::cerr);
}

//****************************************************************************

// Verify that all metrics within the set 's' have the same period.
// Otherwise, things get complicated...
static bool 
VerifyPeriod(const PCProfileMetricSet* s)
{
  if (s->GetSz() == 0) { return true; }
  
  ulong period = (*s)[0]->GetPeriod();
  for (PCProfileMetricSetIterator it(*s); it.IsValid(); ++it) {
    PCProfileMetric* m = it.Current();
    if (period != m->GetPeriod()) {
      return false;
    }
  }
  return true;
}

static ulong 
GetPeriod(const PCProfileMetricSet* s)
{
  // We assume that if s is non-empty, all the metrics periods are the same!
  if (s->GetSz() == 0) { 
    return 0;
  } else {
    return (*s)[0]->GetPeriod();
  }
}

String
GetNativeName(const PCProfileMetricSet* s)
{
  String nm;
  PCProfileMetricSetIterator it(*s);
  for (uint i = 0; it.IsValid(); ++it, ++i) {
    PCProfileMetric* m = it.Current();
    if (i != 0) { nm += "+"; }
    nm += String("[") + m->GetName() + "]";
  }
  
  if (nm.Empty()) { nm = "<no-metrics>"; }
  return nm;
}
