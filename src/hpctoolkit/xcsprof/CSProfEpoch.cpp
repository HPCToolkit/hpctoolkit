// -*-Mode: C++;-*-
// $Id$

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
//   $Source$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "CSProfEpoch.hpp"

#include <lib/xml/xml.hpp>
using namespace xml;

//*************************** Forward Declarations ***************************

//****************************************************************************

CSProfLDmodule::CSProfLDmodule()
{
  lm = NULL;
}

CSProfLDmodule::~CSProfLDmodule()
{
  delete lm;
}


CSProfEpoch::CSProfEpoch(const unsigned int i)
  : loadmoduleVec(i)
{
  numberofldmodule = i;
} 


CSProfEpoch::~CSProfEpoch()
{
  for (CSProfEpoch_LdModuleIterator it(*this); it.IsValid(); ++it) {
    CSProfLDmodule* lm = it.Current(); 
    delete lm; 
  }
  
  loadmoduleVec.clear();
}

CSProfLDmodule* 
CSProfEpoch::FindLDmodule(VMA ip)
{
  CSProfLDmodule* pre=loadmoduleVec[0];
  for (int i=0; i< numberofldmodule; i++) { 
    CSProfLDmodule* curr =loadmoduleVec[i];
    if (ip >= (pre->GetMapaddr()) &&
	ip < curr->GetMapaddr())
      return pre;
    else 
      pre = curr;
  }
  
  return pre;
}

void CSProfLDmodule::Dump(std::ostream& o)
{ 
  using std::hex;
  using std::dec;
  using std::endl; 
  
  o<<"the load module name is " << name;
  o<<" vaddr is 0x" << hex  << vaddr;
  o<<" mapaddr is 0x" << hex <<  mapaddr;
  o<< dec << endl; 
}

void CSProfLDmodule::DDump()
{
  Dump(std::cerr);
}

void CSProfEpoch::Dump(std::ostream& o)
{
  for (int i=0; i<numberofldmodule; i++) {
    CSProfLDmodule* lm = loadmoduleVec[i];
    lm->Dump(o);
  }
}


void CSProfEpoch::DDump()
{
  Dump(std::cerr);
}


