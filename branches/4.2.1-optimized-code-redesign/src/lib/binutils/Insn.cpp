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

#include <iostream>
using std::hex;
using std::dec;

#include <string>
using std::string;

//*************************** User Include Files ****************************

#include "Insn.hpp"

//*************************** Forward Declarations ***************************

//****************************************************************************

//****************************************************************************
// Instruction
//****************************************************************************

void
binutils::Insn::Dump(std::ostream& o, const char* pre) const
{
  string p(pre);
  VMA target = GetTargetVMA(vma);

  o << p << hex << "0x" << vma << dec << ": " << GetDesc().ToString();

  if (target != 0 || GetOpIndex() != 0) { 
    o << " <0x" << hex << target << dec << "> "; 
  }
  else { 
    o << " "; 
  }

  DumpSelf(o, p.c_str());
  o << "\n";
}


void
binutils::Insn::DDump() const
{
  Dump(std::cerr);
}


void
binutils::Insn::DumpSelf(std::ostream& o, const char* pre) const
{
}


//***************************************************************************
// CISCInsn
//***************************************************************************

void
binutils::CISCInsn::Dump(std::ostream& o, const char* pre) const
{
  Insn::Dump(o, pre);
}


void
binutils::CISCInsn::DDump() const
{
  Dump(std::cerr);
}


void
binutils::CISCInsn::DumpSelf(std::ostream& o, const char* pre) const
{
  o << "(CISC sz:" << GetSize() << ")";
}


//***************************************************************************
// RISCInsn
//***************************************************************************

void
binutils::RISCInsn::Dump(std::ostream& o, const char* pre) const
{
  Insn::Dump(o, pre);
}


void
binutils::RISCInsn::DDump() const
{
  Dump(std::cerr);
}


void
binutils::RISCInsn::DumpSelf(std::ostream& o, const char* pre) const
{
  o << "(RISC)";
}


//***************************************************************************
// VLIWInsn
//***************************************************************************

void
binutils::VLIWInsn::Dump(std::ostream& o, const char* pre) const
{
  Insn::Dump(o, pre);
}


void
binutils::VLIWInsn::DDump() const
{
  Dump(std::cerr);
}


void
binutils::VLIWInsn::DumpSelf(std::ostream& o, const char* pre) const
{
  o << "(VLIW opIdx:" << GetOpIndex() << ")";
}
