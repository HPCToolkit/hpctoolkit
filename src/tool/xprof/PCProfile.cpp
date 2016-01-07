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
//    PCProfile.C
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

#include "PCProfile.hpp"

//*************************** Forward Declarations ***************************

using std::endl;
using std::hex;
using std::dec;

//****************************************************************************
// PCProfileMetricSet
//****************************************************************************

PCProfileMetricSet::PCProfileMetricSet(ISA* isa_, unsigned sz)
  : isa(isa_)
{
  metricVec.reserve(16);
  isa->attach();
}

PCProfileMetricSet::~PCProfileMetricSet()
{
  for (unsigned int i = 0; i < GetSz(); i++) {
    delete metricVec[i];
  }
  metricVec.clear();
  isa->detach();
}

int
PCProfileMetricSet::DataExists(VMA pc, ushort opIndex) const
{
  for (unsigned int i = 0; i < GetSz(); i++) {
    const PCProfileMetric* m = Index(i);
    if (m->Find(pc, opIndex) != PCProfileDatum_NIL) {
      return (int)i; // this cast should never be a problem
    }
  }
  return -1;
}

PCProfileMetricSet*
PCProfileMetricSet::Filter(MetricFilter* filter) const
{
  PCProfileMetricSet* s = new PCProfileMetricSet(isa);
  
  for (unsigned int i = 0; i < GetSz(); i++) {
    PCProfileMetric* m = metricVec[i];
    if ( (*filter)(m) ) {
      s->Add(m);
    }
  }
  return s;
}

void 
PCProfileMetricSet::dump(std::ostream& o)
{
  o << "'PCProfileMetricSet' --\n";
  o << "  vec size: " << GetSz() << "\n";
  for (unsigned int i = 0; i < GetSz(); i++) {
    metricVec[i]->dump(o);
  }
}

void 
PCProfileMetricSet::ddump()
{
  dump(std::cerr);
}


//****************************************************************************
// PCProfile
//****************************************************************************

PCProfile::PCProfile(ISA* isa_, unsigned int sz)
  : PCProfileMetricSet(isa_, sz)
{
  pcVec.reserve(1024);
}

PCProfile::~PCProfile()
{
  pcVec.clear();
}

void
PCProfile::AddPC(VMA pc, ushort opIndex) 
{
  if (pcVec.size() == pcVec.capacity()) {
    pcVec.reserve(pcVec.capacity() * 2);
  }
  VMA oppc = GetISA()->convertVMAToOpVMA(pc, opIndex);
  pcVec.push_back(oppc);
}

void 
PCProfile::dump(std::ostream& o)
{
  o << "'PCProfile' --\n";
  o << "  file: " << profiledFile << "\n";
  o << "  header info:\n" << fHdrInfo;
  PCProfileMetricSet::dump(o);  
}

void 
PCProfile::ddump()
{
  dump(std::cerr);
}

//****************************************************************************
// PCProfileVec
//****************************************************************************

PCProfileVec::PCProfileVec(unsigned int sz)
  : datum(0), vec(sz, PCProfileDatum_NIL)
{
}

bool 
PCProfileVec::IsZeroed()
{
  for (ulong i = 0; i < vec.size(); i++) {
    if (vec[i] != PCProfileDatum_NIL) { return false; }
  }
  return true; // every entry is '0'
}

void 
PCProfileVec::dump(std::ostream& o)
{
  o << "'PCProfileVec' --\n";
  o << "  datum=" << datum << endl;
  o << "  vec=[";
  for (unsigned int i = 0; i < vec.size(); i++) {
    if (i != 0) { o << ", "; }
    o << vec[i];
  }
  o << "]" << endl;
}

void 
PCProfileVec::ddump()
{
  dump(std::cerr);
}
