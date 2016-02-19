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

#include <sstream>

#include <string>
using std::string;

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>

#include "Insn.hpp"

//*************************** Forward Declarations ***************************

//****************************************************************************

//****************************************************************************
// Instruction
//****************************************************************************

void
BinUtil::Insn::dump(std::ostream& o, int GCC_ATTR_UNUSED flags,
		    const char* pre) const
{
  string p(pre);
  VMA target = targetVMA(m_vma);

  o << showbase 
    << p << hex << m_vma << dec << ": " << desc().toString();

  if (target != 0 || opIndex() != 0) { 
    o << " <" << hex << target << dec << "> "; 
  }
  else { 
    o << " "; 
  }

  dumpme(o, p.c_str());
  o << "\n";
}


string
BinUtil::Insn::toString(int flags, const char* pre) const
{
  std::ostringstream os;
  dump(os, flags, pre);
  return os.str();
}


void
BinUtil::Insn::ddump() const
{
  dump(std::cerr);
}


void
BinUtil::Insn::dumpme(std::ostream& GCC_ATTR_UNUSED o,
		      const char* GCC_ATTR_UNUSED pre) const
{
}


//***************************************************************************
// CISCInsn
//***************************************************************************

void
BinUtil::CISCInsn::dump(std::ostream& o, int flags, const char* pre) const
{
  Insn::dump(o, flags, pre);
}


void
BinUtil::CISCInsn::dumpme(std::ostream& o,
			  const char* GCC_ATTR_UNUSED pre) const
{
  o << "(CISC sz:" << size() << ")";
}


//***************************************************************************
// RISCInsn
//***************************************************************************

void
BinUtil::RISCInsn::dump(std::ostream& o, int flags, const char* pre) const
{
  Insn::dump(o, flags, pre);
}


void
BinUtil::RISCInsn::dumpme(std::ostream& o,
			  const char* GCC_ATTR_UNUSED pre) const
{
  o << "(RISC)";
}


//***************************************************************************
// VLIWInsn
//***************************************************************************

void
BinUtil::VLIWInsn::dump(std::ostream& o, int flags, const char* pre) const
{
  Insn::dump(o, flags, pre);
}


void
BinUtil::VLIWInsn::dumpme(std::ostream& o,
			  const char* GCC_ATTR_UNUSED pre) const
{
  o << "(VLIW opIdx:" << opIndex() << ")";
}
