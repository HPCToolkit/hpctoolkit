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
//    PCProfileMetric.C
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

#include "PCProfileMetric.hpp"

//*************************** Forward Declarations ***************************

//****************************************************************************
// PCProfileMetric
//****************************************************************************

PCProfileMetric::PCProfileMetric(ISA* isa_)
  : total(0), period (0), txtStart(0), txtSz(0), isa(isa_)
{ 
  /* map does not need to be initialized */ 
  isa->attach();
}

PCProfileMetric::~PCProfileMetric() 
{ 
  isa->detach();
}

void 
PCProfileMetric::dump(std::ostream& o)
{
  o << "'PCProfileMetric' --\n";
  o << "  name: " << name << "\n";
  o << "  textStart (hex): " << hex << txtStart << dec << "\n";
  o << "  textSize: " << txtSz << "\n";
  o << "  map size (number of entries): " << map.size() << "\n";
  o << "  map entries (PC is reported in hex as an offset from textStart) = [";
  VMA pc = 0;
  PCProfileDatum d = 0;
  unsigned int printed = 0;
  for (PCToPCProfileDatumMapIt it = map.begin(); it != map.end(); ++it) {
    if (printed != 0) { o << ", "; }
    pc = (*it).first - txtStart; // actually, the offset
    d = (*it).second;
    o << "(" << hex << pc << dec << ", " << d << ")";
    printed++;
  }
  o << "]" << endl;
}

void
PCProfileMetric::ddump()
{
  dump(std::cerr);
}

