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
//   A derivation of the IR interface for the ISA (disassembler) class
//   of Struct.
//
//   Note: many stubs still exist.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef BAnal_OAInterface_hpp
#define BAnal_OAInterface_hpp

//************************* System Include Files ****************************

#include <list>
#include <set>

//************************ OpenAnalysis Include Files ***********************

#include <OpenAnalysis/IRInterface/CFGIRInterfaceDefault.hpp>
#include <OpenAnalysis/CFG/CFGInterface.hpp>

#include "OpenAnalysis/Utils/OutputBuilderText.hpp"
#include "OpenAnalysis/Utils/OutputBuilderDOT.hpp"

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
 
#include <lib/isa/ISA.hpp>

#include <lib/binutils/LM.hpp>
#include <lib/binutils/Proc.hpp>
#include <lib/binutils/Insn.hpp>

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

// IRInterface types: Use OA_IRHANDLETYPE_SZ64 (size of bfd_vma/VMA)
//   ProcHandle  - binutils::Proc*
//   StmtHandle  - binutils::Insn*
//   ExprHandle  -
//   LeafHandle  -
//   StmtLabel   - VMA
//   SymHandle   - char* (simply dummy strings)
//   ConstHandle -

// FIXME: eraxxon: Due to some unwariness, these types are a mixture
// of fixed size (VMA) and relative size (binutils::Insn*).  I think we
// should be able to use only one so that casts are always between
// types of the same size.

// The IR handle type is 64 bits but we will need to interface with
// 32-bit pointers at times.  Use this macro to eliminate potential
// compiler warnings about "casting a 32-bit pointer to an integer of
// different size".
#define TY_TO_IRHNDL(x, totype) (totype((OA::irhandle_t)(uintptr_t)(x)))
#define IRHNDL_TO_TY(x, totype) ((totype)(uintptr_t)(x.hval()))

//***************************************************************************
// 
//***************************************************************************

namespace BAnal {

inline BinUtil::Insn*
OA_CFG_getBegInsn(OA::OA_ptr<OA::CFG::NodeInterface> bb)
{
  OA::OA_ptr<OA::CFG::NodeStatementsIteratorInterface> stmtIt =
    bb->getNodeStatementsIterator();
  
  BinUtil::Insn* stmt = NULL;
  if (stmtIt->isValid()) {
    stmt = IRHNDL_TO_TY(stmtIt->current(), BinUtil::Insn*);
  }
  return stmt;
}


inline BinUtil::Insn*
OA_CFG_getEndInsn(OA::OA_ptr<OA::CFG::NodeInterface> bb)
{
  OA::OA_ptr<OA::CFG::NodeStatementsRevIteratorInterface> stmtIt =
    bb->getNodeStatementsRevIterator();
  BinUtil::Insn* stmt = NULL;
  if (stmtIt->isValid()) {
    stmt = IRHNDL_TO_TY(stmtIt->current(), BinUtil::Insn*);
  }
  return stmt;
}

} // end namespace BAnal


//***************************************************************************
// Iterators
//***************************************************************************

namespace BAnal {

class RegionStmtIterator: public OA::IRRegionStmtIterator {
public:
  RegionStmtIterator(BinUtil::Proc& _p)
    : pii(_p)
  { }

  virtual ~RegionStmtIterator()
  { }

  virtual OA::StmtHandle
  current () const
  { return TY_TO_IRHNDL(pii.current(), OA::StmtHandle); }

  virtual bool
  isValid () const
  { return pii.isValid(); }
  
  virtual void
  operator++ ()
  { ++pii; }

  virtual void
  reset() { pii.reset(); }

private:
  BinUtil::ProcInsnIterator pii;
};

}

//***************************************************************************
// Abstract Interfaces
//***************************************************************************

namespace BAnal {

class OAInterface
  : public virtual OA::IRHandlesIRInterface,
    public OA::CFG::CFGIRInterfaceDefault
{
public:

  // Note: We assume each instantiation of the IRInterface represents
  // one procedure!
  OAInterface(BinUtil::Proc* proc);
  virtual ~OAInterface();
  
  
  //-------------------------------------------------------------------------
  // IRHandlesIRInterface
  //-------------------------------------------------------------------------
  
  // create a string for the given handle, should be succinct
  // and there should be no newlines
  std::string toString(const OA::ProcHandle h);
  std::string toString(const OA::CallHandle h);
  std::string toString(const OA::StmtHandle h);
  std::string toString(const OA::ExprHandle h);
  std::string toString(const OA::OpHandle h);
  std::string toString(const OA::MemRefHandle h);
  std::string toString(const OA::SymHandle h);
  std::string toString(const OA::ConstSymHandle h);
  std::string toString(const OA::ConstValHandle h);
  
  // Given a statement, pretty-print it to the output stream os.
  void
  dump(OA::StmtHandle stmt, std::ostream& os);
  
  // Given a memory reference, pretty-print it to the output stream os.
  void
  dump(OA::MemRefHandle h, std::ostream& os);

  void
  currentProc(OA::ProcHandle p);

  //-------------------------------------------------------------------------
  // CFGIRInterfaceDefault
  //-------------------------------------------------------------------------
  
  //! Given a ProcHandle, return an IRRegionStmtIterator* for the
  //! procedure. The user must free the iterator's memory via delete.
  OA::OA_ptr<OA::IRRegionStmtIterator>
  procBody(OA::ProcHandle h);


  //--------------------------------------------------------
  // Statements: General
  //--------------------------------------------------------

  //! Are return statements allowed
  bool
  returnStatementsAllowed()
  { return true; }

  //! Given a statement, return its CFG::IRStmtType
  OA::CFG::IRStmtType
  getCFGStmtType(OA::StmtHandle h);

  OA::StmtLabel
  getLabel(OA::StmtHandle h);

  OA::OA_ptr<OA::IRRegionStmtIterator>
  getFirstInCompound(OA::StmtHandle h);


  //--------------------------------------------------------
  // Loops
  //--------------------------------------------------------
  OA::OA_ptr<OA::IRRegionStmtIterator>
  loopBody(OA::StmtHandle h);

  OA::StmtHandle
  loopHeader(OA::StmtHandle h);

  OA::StmtHandle
  getLoopIncrement(OA::StmtHandle h);

  bool
  loopIterationsDefinedAtEntry(OA::StmtHandle h);

  //--------------------------------------------------------
  // Structured two-way conditionals
  //--------------------------------------------------------
  OA::OA_ptr<OA::IRRegionStmtIterator>
  trueBody(OA::StmtHandle h);

  OA::OA_ptr<OA::IRRegionStmtIterator>
  elseBody(OA::StmtHandle h);
  
  //--------------------------------------------------------
  // Structured multiway conditionals
  //--------------------------------------------------------
  int
  numMultiCases(OA::StmtHandle h);

  OA::OA_ptr<OA::IRRegionStmtIterator>
  multiBody(OA::StmtHandle h, int bodyIndex);

  bool
  isBreakImplied(OA::StmtHandle h);

  bool
  isCatchAll(OA::StmtHandle h, int bodyIndex);

  OA::OA_ptr<OA::IRRegionStmtIterator>
  getMultiCatchall(OA::StmtHandle h);

  OA::ExprHandle
  getSMultiCondition(OA::StmtHandle h, int bodyIndex);

  //--------------------------------------------------------
  // Unstructured two-way conditionals
  //--------------------------------------------------------
  OA::StmtLabel
  getTargetLabel(OA::StmtHandle h, int n);
  
  //--------------------------------------------------------
  // Unstructured multi-way conditionals
  //--------------------------------------------------------
  int
  numUMultiTargets(OA::StmtHandle h);

  OA::StmtLabel
  getUMultiTargetLabel(OA::StmtHandle h, int targetIndex);

  OA::StmtLabel
  getUMultiCatchallLabel(OA::StmtHandle h);

  OA::ExprHandle
  getUMultiCondition(OA::StmtHandle h, int targetIndex);

  //--------------------------------------------------------
  // Special
  //--------------------------------------------------------
  bool
  parallelWithSuccessor(OA::StmtHandle GCC_ATTR_UNUSED h) { return false; }

  int
  numberOfDelaySlots(OA::StmtHandle h);

  //--------------------------------------------------------
  // Symbol Handles
  //--------------------------------------------------------
  OA::SymHandle
  getProcSymHandle(OA::ProcHandle h);
  
private:
  OAInterface() { DIAG_Die(DIAG_Unimplemented); }

  // Given a VMA 'vma', compute the closest actual target VMA.
  // Technically, this should never be necessary, because decoding
  // should be correct.  However, this is much easier than upgrading
  // to the next version of binutils every time new x86_64
  // instructions are added.
  VMA
  normalizeTarget(VMA vma) const
  {
    VMA vma_norm = vma;
    BinUtil::Insn* insn = m_proc->lm()->findInsnNear(vma, 0);
    if (insn) {
      // could check that (vma_norm - vma) is no greater than a few bytes
      vma_norm = insn->vma();
    }
    return vma_norm;
  }

private:
  BinUtil::Proc* m_proc;
  std::set<VMA> m_branchTargetSet;
};

} // namespace BAnal

#endif // BAnal_OAInterface_hpp
