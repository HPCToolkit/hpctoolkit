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
//   $HeadURL$
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
using std::showbase;
using std::endl;

#include <string>
using std::string;

#include <sstream>

//*************************** User Include Files ****************************

#include "Proc.hpp"
#include "Insn.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// Proc
//***************************************************************************

unsigned int BinUtil::Proc::nextId = 0;

BinUtil::Proc::Proc(BinUtil::TextSeg* seg,
		    const string& name, const string& linkname,
		    BinUtil::Proc::Type t, VMA begVMA, VMA endVMA,
		    unsigned int size)
  : m_seg(seg), m_name(name), m_linkname(linkname), m_type(t), m_begVMA(begVMA),
    m_endVMA(endVMA), m_size(size), m_filenm(""), m_begLine(0), m_parent(NULL)
{
  m_id = nextId++;
  m_numInsns = 0;
}


BinUtil::Proc::~Proc()
{
  m_seg = NULL;
}


BinUtil::Insn*
BinUtil::Proc::endInsn() const
{
  Insn* insn = findInsn(m_endVMA, 0);
  if (insn) {
    ushort numOps = insn->numOps();
    if (numOps != 0) {
      insn = findInsn(m_endVMA, (ushort)(numOps - 1)); // opIndex is 0-based
    }
  }
  return insn;
}


string
BinUtil::Proc::toString(int flags) const
{
  std::ostringstream os;
  dump(os, flags);
  return os.str();
}


void
BinUtil::Proc::dump(std::ostream& os, int flags, const char* pre) const
{
  string p(pre);
  string p1 = p + "  ";
  string p2 = p + "    ";
  
  string proc, file, b_proc, b_file, e_proc, e_file;
  SrcFile::ln begLn, endLn, b_begLn, e_endLn2;
  Insn* eInsn = endInsn();
  ushort endOp = (eInsn) ? eInsn->opIndex() : (ushort)0;

  // This call performs some consistency checking
  m_seg->findSrcCodeInfo(begVMA(), 0, endVMA(), endOp,
			   proc, file, begLn, endLn);

  // These calls perform no consistency checking
  m_seg->findSrcCodeInfo(begVMA(), 0, b_proc, b_file, b_begLn);
  m_seg->findSrcCodeInfo(endVMA(), endOp, e_proc, e_file, e_endLn2);

  string nm = BinUtil::canonicalizeProcName(name());
  string ln_nm = BinUtil::canonicalizeProcName(linkName());
  
  os << p << "---------- Procedure Dump ----------\n";
  os << p << "  Name:     `" << nm << "'\n";
  os << p << "  LinkName: `" << ln_nm << "'\n";
  os << p << "  Sym:      {" << filename() << "}:" << begLine() << "\n";
  os << p << "  LnMap:    {" << file << "}["
     << BinUtil::canonicalizeProcName(proc) <<"]:" << begLn << "-" << endLn << "\n";
  os << p << "  LnMap(b): {" << b_file << "}["
     << BinUtil::canonicalizeProcName(b_proc) << "]:" << b_begLn << "\n";
  os << p << "  LnMap(e): {" << e_file << "}["
     << BinUtil::canonicalizeProcName(e_proc) << "]:" << e_endLn2 << "\n";
  
  os << p << "  ID, Type: " << id() << ", `";
  switch (type()) {
    case Local:   os << "Local'\n";  break;
    case Weak:    os << "Weak'\n";   break;
    case Global:  os << "Global'\n"; break;
    case Quasi:   os << "Quasi'\n";  break;
    default:      os << "-unknown-'\n";
      DIAG_Die("Unknown Procedure type: " << type());
  }
  os << showbase
     << p << "  VMA: [" << hex << begVMA() << ", " << endVMA() << dec << "]\n";
  os << p << "  Size(b): " << size() << "\n";
  
  if ((flags & LM::DUMP_Flg_Insn_ty)
      || (flags & LM::DUMP_Flg_Insn_decode)) {
    os << p1 << "----- Instruction Dump -----\n";
    for (ProcInsnIterator it(*this); it.isValid(); ++it) {
      Insn* insn = it.current();

      if (flags & LM::DUMP_Flg_Insn_decode) {
	os << p2 << hex << insn->vma() << dec << ": ";
	insn->decode(os);
	os << endl;
      }
      if (flags & LM::DUMP_Flg_Insn_ty) {
	insn->dump(os, flags, p2.c_str());
      }
      
      if (flags & LM::DUMP_Flg_Sym) {
	VMA vma = insn->vma();
	ushort opIdx = insn->opIndex();

	string proc1, file1;
	SrcFile::ln line;
    	m_seg->findSrcCodeInfo(vma, opIdx, proc1, file1, line);
	proc1 = BinUtil::canonicalizeProcName(proc1);
	
	os << p2 << "  ";
	if (file1 == filename()) {
	  os << "-";
	}
	else {
	  os << "!{" << file1 << "}";
	}
	os << ":" << line << ":";
	if (proc1 == nm || proc1 == ln_nm) {
	  os << "-";
	}
	else {
	  os << "![" << proc1 << "]";
	}
	os << "\n";
      }
    }
  }
}


void
BinUtil::Proc::ddump() const
{
  dump(std::cerr);
}


//***************************************************************************
// ProcInsnIterator
//***************************************************************************

BinUtil::ProcInsnIterator::ProcInsnIterator(const Proc& _p)
  : p(_p), lm(*(p.lm()))
{
  reset();
}


BinUtil::ProcInsnIterator::~ProcInsnIterator()
{
}


void
BinUtil::ProcInsnIterator::reset()
{
  it    = lm.m_insnMap.find(p.m_begVMA);
  endIt = lm.m_insnMap.find(p.m_endVMA);
  
  if (it != lm.m_insnMap.end()) {
    // We have at least one instruction: p.endVMA should have been found
    DIAG_Assert(endIt != lm.m_insnMap.end(), "Internal error!");
    
    endIt++; // 'endIt' is now one past the last valid instruction
  
    // We need to ensure that all VLIW instructions that match this
    // vma are also included.  Push 'endIt' back as needed; when done it
    // should remain one past the last valid instruction
    for (; // ((*endIt).second) returns Insn*
	 (endIt != lm.m_insnMap.end() && endIt->second->vma() == p.m_endVMA);
	 endIt++)
      { }
  }
  else {
    // 'it' == end ==> p.begVMA == p.endVMA (but not the reverse)
    DIAG_Assert(p.m_begVMA == p.m_endVMA, "Internal error!");
  }
}
