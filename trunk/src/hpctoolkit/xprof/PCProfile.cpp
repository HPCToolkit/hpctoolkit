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

#include "PCProfile.h"

//*************************** Forward Declarations ***************************

using std::endl;
using std::hex;
using std::dec;

//****************************************************************************
// PCProfileVec
//****************************************************************************

PCProfileVec::PCProfileVec(suint sz)
  : datum(0), vec(std::vector<PCProfileDatum>(sz))
{
  for (suint i = 0; i < vec.size(); i++) {
    vec[i] = PCProfileDatum_NIL;
  }
}

bool PCProfileVec::IsZeroed()
{
  for (suint i = 0; i < vec.size(); i++) {
    if (vec[i] != 0) { return false; } // not every entry is '0'
  }
  return true; // every entry is '0'
}

void PCProfileVec::Dump(std::ostream& o)
{
  o << "'PCProfileVec' --\n";
  o << "  datum=" << datum << endl;
  o << "  vec=[";
  for (suint i = 0; i < vec.size(); i++) {
    if (i != 0) { o << ", "; }
    o << vec[i];
  }
  o << "]" << endl;
}

void PCProfileVec::DDump()
{
  Dump(std::cerr);
}

//****************************************************************************
// PCProfile
//****************************************************************************

PCProfile::PCProfile(suint sz)
  : totalCount(0), metricVec(std::vector<PCProfileMetric*>(sz))
{
  for (suint i = 0; i < metricVec.size(); i++) {
    metricVec[i] = NULL;
  }
}

PCProfile::~PCProfile()
{
  for (suint i = 0; i < metricVec.size(); i++) {
    delete metricVec[i];
  }
}

void PCProfile::Dump(std::ostream& o)
{
  o << "'PCProfile' --\n";
  o << "  vec size: " << metricVec.size() << "\n";
  for (suint i = 0; i < metricVec.size(); i++) {
    metricVec[i]->Dump(o);
  }
}

void PCProfile::DDump()
{
  Dump(std::cerr);
}

//****************************************************************************
// PCProfileMetric
//****************************************************************************

void PCProfileMetric::Dump(std::ostream& o)
{
  o << "'PCProfileMetric' --\n";
  o << "  name: " << name << "\n";
  o << "  textStart (hex): " << hex << txtStart << dec << "\n";
  o << "  textSize: " << txtSz << "\n";
  o << "  map size (number of entries): " << map.size() << "\n";
  o << "  map entries (PC is reported in hex as an offset from textStart) = [";
  Addr pc = 0;
  PCProfileDatum d = 0;
  suint printed = 0;
  for (PCToPCProfileDatumMapIt it = map.begin(); it != map.end(); ++it) {
    if (printed != 0) { o << ", "; }
    pc = (*it).first - txtStart; // actually, the offset
    d = (*it).second;
    o << "(" << hex << pc << dec << ", " << d << ")";
    printed++;
  }
  o << "]" << endl;
}

void PCProfileMetric::DDump()
{
  Dump(std::cerr);
}

