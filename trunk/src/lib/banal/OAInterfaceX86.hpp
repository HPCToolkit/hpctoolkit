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
// Copyright ((c)) 2002-2013, Rice University
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
//   A derivation of the IR interface for the x86 (disassembler) class
//   of Struct.
//
//   Note: many stubs still exist.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef BAnal_OAInterfaceX86_hpp
#define BAnal_OAInterfaceX86_hpp

//************************* System Include Files ****************************

#include <list>
#include <set>

//************************ OpenAnalysis Include Files ***********************

#include <OpenAnalysis/IRInterface/CFGIRInterfaceDefault.hpp>
#include <OpenAnalysis/CFG/CFGInterface.hpp>

#include "OpenAnalysis/Utils/OutputBuilderText.hpp"
#include "OpenAnalysis/Utils/OutputBuilderDOT.hpp"

//*************************** User Include Files ****************************

#include <lib/banal/OAInterface.hpp>

#include <include/gcc-attr.h>
 
#include <lib/isa/ISA.hpp>

#include <lib/binutils/LM.hpp>
#include <lib/binutils/Proc.hpp>
#include <lib/binutils/Insn.hpp>

#include <lib/support/diagnostics.h>

#include "OAInterface.hpp"

//*************************** Forward Declarations ***************************


//***************************************************************************
// 
//***************************************************************************


//***************************************************************************
// Iterators
//***************************************************************************


//***************************************************************************
// Abstract Interfaces
//***************************************************************************

namespace BAnal {

class OAInterfaceX86
  : public BAnal::OAInterface
{
public:

  // Note: We assume each instantiation of the IRInterface represents
  // one procedure!
  OAInterfaceX86(BinUtil::Proc* proc);
  
  //--------------------------------------------------------
  // Statements: General
  //--------------------------------------------------------

  //! Given a statement, return its CFG::IRStmtType
  OA::CFG::IRStmtType
  getCFGStmtType(OA::StmtHandle h);

  
  //--------------------------------------------------------
  // Unstructured two-way conditionals
  //--------------------------------------------------------
  OA::StmtLabel
  getLabel(OA::StmtHandle h);
  
  OA::StmtLabel
  getTargetLabel(OA::StmtHandle h, int GCC_ATTR_UNUSED n);


  //--------------------------------------------------------
  // Symbol Handles
  //--------------------------------------------------------
  OA::SymHandle
  getProcSymHandle(OA::ProcHandle h);
  
};

} // namespace BAnal

#endif // BAnal_OAInterfaceX86_hpp
