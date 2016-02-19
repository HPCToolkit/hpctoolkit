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
using std::endl;
using std::hex;
using std::dec;

#include <string>
using std::string;

//*************************** User Include Files ****************************

#include "DerivedProfile.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

static bool 
VerifyPeriod(const PCProfileMetricSet* s);

static ulong 
GetPeriod(const PCProfileMetricSet* s);

string
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
    for (unsigned int i = 0; i < pcprof->GetNumMetrics(); ++i) {
      const PCProfileMetric* m = pcprof->GetMetric(i);
      DerivedProfileMetric* dm = new DerivedProfileMetric(m);
      SetMetric(i, dm);
    }
  } else {
    // For each metric filter, create a derived metric
    SetNumMetrics(filtlist->size());
    PCProfileFilterList::const_iterator it = filtlist->begin();
    for (unsigned int i = 0; it != filtlist->end(); ++it, ++i) {
      PCProfileFilter* filt = *it;
      MetricFilter* mfilt = filt->GetMetricFilter();
      const PCProfileMetricSet* s = pcprof->Filter(mfilt);
      DIAG_Assert(VerifyPeriod(s), ""); // we should all have the same period
      
      DerivedProfileMetric* dm = new DerivedProfileMetric(s);
      dm->SetName(filt->GetName());
      dm->SetNativeName(GetNativeName(s));
      dm->SetDescription(filt->GetDescription());
      dm->SetPeriod(GetPeriod(s));
      SetMetric(i, dm);
    }
  }
  
  // -------------------------------------------------------
  // 2. Determine set of interesting PCs.  As a special case, if
  // filtlist is null, we have a one to one mapping between raw PCs
  // and derived PCs.
  // -------------------------------------------------------
  
  if (filtlist == NULL) {
    for (DerivedProfile_MetricIterator dmIt(*this); dmIt.IsValid(); ++dmIt) {
      DerivedProfileMetric* dm = dmIt.Current();
      dm->MakeDerivedPCSetCoterminousWithPCSet();
    }
  } else {
    // FIXME: Save memory by using MakeDerivedPCSetCoterminousWithPCSet() 
    // when the insn filter is the identity filter.
    
    // Iterate over VMA values in the profile
    for (PCProfile_PCIterator it(*pcprof); it.IsValid(); ++it) {
      
      VMA opvma = it.Current(); // an 'operation vma'
      ushort opIndex;
      VMA vma = binutils::LM::isa->convertOpVMAToVMA(opvma, opIndex);
      
      // For each derived metric and its insn filter
      PCProfileFilterList::const_iterator fIt = filtlist->begin();
      for (unsigned int i = 0; fIt != filtlist->end(); ++fIt, ++i) {
	PCProfileFilter* filt = *fIt;
	PCFilter* pcfilt = filt->GetPCFilter();
	
	DerivedProfileMetric* dm 
	  = const_cast<DerivedProfileMetric*>(GetMetric(i));
	const PCProfileMetricSet* rawMSet = dm->GetMetricSet();
	
	// if vma has data && insn filter is satisfied for vma, record vma
	if (rawMSet->DataExists(vma, opIndex) >= 0 && (*pcfilt)(vma, opIndex)) {
	  dm->InsertPC(vma, opIndex);
	}
      }  
    }
  }

}

void 
DerivedProfile::Dump(std::ostream& o)
{
  o << "'DerivedProfile' --\n";
  if (pcprof) {
    const_cast<PCProfile*>(pcprof)->dump(o);
  }
  
  o << "'Metrics' --\n";
  for (DerivedProfile_MetricIterator it(*this); it.IsValid(); ++it) {
    DerivedProfileMetric* dm = it.Current();
    dm->Dump(o);
  }
}

void 
DerivedProfile::DDump()
{
  Dump(std::cerr);
}

//****************************************************************************
// DerivedProfileMetric
//****************************************************************************

DerivedProfileMetric::DerivedProfileMetric(const PCProfileMetricSet* s)
{ 
  Ctor(s);
}

DerivedProfileMetric::DerivedProfileMetric(const PCProfileMetric* m)
{
  PCProfileMetricSet* s = new PCProfileMetricSet(m->GetISA(), 1);
  s->Add(m);

  Ctor(s);
  SetName(m->GetName());
  SetNativeName(m->GetName());
  SetDescription(m->GetDescription());
  SetPeriod(m->GetPeriod());
}

void
DerivedProfileMetric::Ctor(const PCProfileMetricSet* s)
{
  SetMetricSet(s); 
  pcset = new PCSet;
}

DerivedProfileMetric::~DerivedProfileMetric()
{
  mset->Clear(); // we own the set container, but not the contents!
  delete mset;
  delete pcset;
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

string
GetNativeName(const PCProfileMetricSet* s)
{
  string nm;
  PCProfileMetricSetIterator it(*s);
  for (unsigned int i = 0; it.IsValid(); ++it, ++i) {
    PCProfileMetric* m = it.Current();
    if (i != 0) { nm += "+"; }
    nm += "[" + m->GetName() + "]";
  }
  
  if (nm.empty()) { nm = "[no-matching-metrics]"; }
  return nm;
}
