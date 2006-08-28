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

#include "Proc.hpp"
#include "Insn.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// Proc
//***************************************************************************

suint binutils::Proc::nextId = 0;

binutils::Proc::Proc(binutils::TextSeg* _sec, string& _name, string& _linkname,
                     binutils::Proc::Type t, VMA _begVMA, VMA _endVMA, 
		     suint _size)
  : sec(_sec), name(_name), linkname(_linkname), type(t), begVMA(_begVMA),
    endVMA(_endVMA), size(_size), filenm(""), begLine(0), parent(NULL)
{
  id = nextId++;
  numInsns = 0; // FIXME: this is never computed
}


binutils::Proc::~Proc()
{
  sec = NULL;
}


binutils::Insn* 
binutils::Proc::GetLastInsn() const
{
  Insn* insn = GetInsn(endVMA, 0);
  if (insn) {
    ushort numOps = insn->GetNumOps();
    if (numOps != 0) {
      insn = GetInsn(endVMA, numOps - 1); // opIndex is 0-based
    }
  }
  return insn;
}


void
binutils::Proc::Dump(std::ostream& o, const char* pre) const
{
  string p(pre);
  string p1 = p + "  ";
  string p2 = p + "    ";  
  
  string func, file, func1, file1, func2, file2;
  suint begLn, endLn, begLn1, endLn2;
  Insn* eInsn = GetLastInsn();
  ushort endOp = (eInsn) ? eInsn->GetOpIndex() : 0;

  // This call performs some consistency checking
  sec->GetSourceFileInfo(GetBegVMA(), 0, GetEndVMA(), endOp,
			 func, file, begLn, endLn);

  // These calls perform no consistency checking
  sec->GetSourceFileInfo(GetBegVMA(), 0, func1, file1, begLn1);
  sec->GetSourceFileInfo(GetEndVMA(), endOp, func2, file2, endLn2);
  
  o << p << "---------- Procedure Dump ----------\n";
  o << p << "  Name:     `" << GetBestFuncName(GetName()) << "'\n";
  o << p << "  LinkName: `" << GetBestFuncName(GetLinkName()) << "'\n";
  o << p << "  DbgNm(s): `" << GetBestFuncName(func1) << "'\n";
  o << p << "  DbgNm(e): `" << GetBestFuncName(func2) << "'\n";
  o << p << "  File:    `" << file << "':" << begLn << "-" << endLn << "\n";
  o << p << "  DbgF(s): `" << file1 << "':" << begLn1 << "\n";
  o << p << "  DbgF(e): `" << file2 << "':" << endLn2 << "\n";

  o << p << "  ID, Type: " << GetId() << ", `";
  switch (GetType()) {
    case Local:   o << "Local'\n";  break;
    case Weak:    o << "Weak'\n";   break;
    case Global:  o << "Global'\n"; break;
    default:      o << "-unknown-'\n"; 
      DIAG_Die("Unknown Procedure type: " << GetType());
  }
  o << p << "  VMA: [0x" << hex << GetBegVMA() << ", 0x"
    << GetEndVMA() << dec << "]\n";
  o << p << "  Size(b): " << GetSize() << "\n";

  o << p1 << "----- Instruction Dump -----\n";
  for (ProcInsnIterator it(*this); it.IsValid(); ++it) {
    Insn* insn = it.Current();
    insn->Dump(o, p2.c_str());
  }
}


void
binutils::Proc::DDump() const
{
  Dump(std::cerr);
}


//***************************************************************************
// ProcInsnIterator
//***************************************************************************

binutils::ProcInsnIterator::ProcInsnIterator(const Proc& _p)
  : p(_p), lm(*(p.GetLM()))
{
  Reset();
}


binutils::ProcInsnIterator::~ProcInsnIterator()
{
}


void
binutils::ProcInsnIterator::Reset()
{
  it    = lm.vmaToInsnMap.find(p.begVMA);
  endIt = lm.vmaToInsnMap.find(p.endVMA); 
  
  if (it != lm.vmaToInsnMap.end()) {
    // We have at least one instruction: p.endVMA should have been found
    DIAG_Assert(endIt != lm.vmaToInsnMap.end(), "Internal error!");
    
    endIt++; // 'endIt' is now one past the last valid instruction
  
    // We need to ensure that all VLIW instructions that match this
    // vma are also included.  Push 'endIt' back as needed; when done it
    // should remain one past the last valid instruction
    for (; // ((*endIt).second) returns Insn*
	 (endIt != lm.vmaToInsnMap.end() 
	  && endIt->second->GetVMA() == p.endVMA);
	 endIt++)
      { }
  }
  else {
    // 'it' == end ==> p.begVMA == p.endVMA (but not the reverse)
    DIAG_Assert(p.begVMA == p.endVMA, "Internal error!");
  }
}
