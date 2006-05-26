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
//    Instruction.C
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

#include "Instruction.hpp"

//*************************** Forward Declarations ***************************

using std::hex;
using std::dec;

//****************************************************************************

//****************************************************************************
// Instruction
//****************************************************************************

void
Instruction::Dump(std::ostream& o, const char* pre) const
{
  String p(pre);
  Addr target = GetTargetAddr(pc);

  o << p << hex << "0x" << pc << dec << ": " << GetDesc().ToString();

  if (target != 0 || GetOpIndex() != 0) { 
    o << " <0x" << hex << target << dec << "> "; 
  }
  else { 
    o << " "; 
  }

  DumpSelf(o, p);
  o << "\n";
}

void
Instruction::DDump() const
{
  Dump(std::cerr);
}

void
Instruction::DumpSelf(std::ostream& o, const char* pre) const
{
}

//***************************************************************************
// CISCInstruction
//***************************************************************************

void
CISCInstruction::Dump(std::ostream& o, const char* pre) const
{
  Instruction::Dump(o, pre);
}

void
CISCInstruction::DDump() const
{
  Dump(std::cerr);
}

void
CISCInstruction::DumpSelf(std::ostream& o, const char* pre) const
{
  o << "(CISC sz:" << GetSize() << ")";
}

//***************************************************************************
// RISCInstruction
//***************************************************************************

void
RISCInstruction::Dump(std::ostream& o, const char* pre) const
{
  Instruction::Dump(o, pre);
}

void
RISCInstruction::DDump() const
{
  Dump(std::cerr);
}

void
RISCInstruction::DumpSelf(std::ostream& o, const char* pre) const
{
  o << "(RISC)";
}

//***************************************************************************
// VLIWInstruction
//***************************************************************************

void
VLIWInstruction::Dump(std::ostream& o, const char* pre) const
{
  Instruction::Dump(o, pre);
}

void
VLIWInstruction::DDump() const
{
  Dump(std::cerr);
}

void
VLIWInstruction::DumpSelf(std::ostream& o, const char* pre) const
{
  o << "(VLIW opIdx:" << GetOpIndex() << ")";
}
