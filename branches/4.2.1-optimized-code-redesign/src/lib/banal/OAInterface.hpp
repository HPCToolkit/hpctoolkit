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
//   A derivation of the IR interface for the ISA (disassembler) class
//   of bloop.
//
//   Note: many stubs still exist.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef banal_OAInterface_hpp
#define banal_OAInterface_hpp

//************************* System Include Files ****************************

#include <list>
#include <set>

//************************ OpenAnalysis Include Files ***********************

#include <OpenAnalysis/IRInterface/CFGIRInterfaceDefault.hpp>
#include <OpenAnalysis/CFG/Interface.hpp>

//*************************** User Include Files ****************************
 
#include <lib/isa/ISA.hpp>

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
#define TY_TO_IRHNDL(x, totype) (totype((OA::irhandle_t)(psuint)(x)))
#define IRHNDL_TO_TY(x, totype) ((totype)(psuint)(x.hval()))

//***************************************************************************
// 
//***************************************************************************

namespace banal {

inline binutils::Insn*
OA_CFG_getBegInsn(OA::OA_ptr<OA::CFG::Interface::Node> bb) 
{
  OA::OA_ptr<OA::CFG::Interface::NodeStatementsIterator> stmtIt =
    bb->getNodeStatementsIterator();
  
  binutils::Insn* stmt = NULL;
  if (stmtIt->isValid()) {
    stmt = IRHNDL_TO_TY(stmtIt->current(), binutils::Insn*);
  }
  return stmt;
}


inline binutils::Insn*
OA_CFG_getEndInsn(OA::OA_ptr<OA::CFG::Interface::Node> bb) 
{
  OA::OA_ptr<OA::CFG::Interface::NodeStatementsRevIterator> stmtIt =
    bb->getNodeStatementsRevIterator();
  binutils::Insn* stmt = NULL;
  if (stmtIt->isValid()) {
    stmt = IRHNDL_TO_TY(stmtIt->current(), binutils::Insn*);
  }
  return stmt;
}

} // end namespace banal


//***************************************************************************
// Iterators
//***************************************************************************

namespace banal {

class RegionStmtIterator: public OA::IRRegionStmtIterator {
public:
  RegionStmtIterator(binutils::Proc& _p) : pii(_p) { }
  virtual ~RegionStmtIterator() { }

  virtual OA::StmtHandle current () const 
    { return TY_TO_IRHNDL(pii.Current(), OA::StmtHandle); }

  virtual bool isValid () const { return pii.IsValid(); }
  virtual void operator++ () { ++pii; }

  virtual void reset() { pii.Reset(); }

private:
  binutils::ProcInsnIterator pii;
};

}

//***************************************************************************
// Abstract Interfaces
//***************************************************************************

namespace banal {

class OAInterface 
  : public virtual OA::IRHandlesIRInterface,
    public OA::CFG::CFGIRInterfaceDefault 
{
public:

  // Note: We assume each instantiation of the IRInterface represents
  // one procedure!
  OAInterface (binutils::Proc* _p);
  virtual ~OAInterface ();
  
  
  //-------------------------------------------------------------------------
  // IRHandlesIRInterface
  //-------------------------------------------------------------------------
  
  // create a string for the given handle, should be succinct
  // and there should be no newlines
  std::string toString(const OA::ProcHandle h);
  std::string toString(const OA::StmtHandle h);
  std::string toString(const OA::ExprHandle h);
  std::string toString(const OA::OpHandle h);
  std::string toString(const OA::MemRefHandle h);
  std::string toString(const OA::SymHandle h);
  std::string toString(const OA::ConstSymHandle h);
  std::string toString(const OA::ConstValHandle h);
  
  // Given a statement, pretty-print it to the output stream os.
  void dump(OA::StmtHandle stmt, std::ostream& os);
  
  // Given a memory reference, pretty-print it to the output stream os.
  void dump(OA::MemRefHandle h, std::ostream& os);

  void currentProc(OA::ProcHandle p);

  //-------------------------------------------------------------------------
  // CFGIRInterfaceDefault
  //-------------------------------------------------------------------------
  
  //! Given a ProcHandle, return an IRRegionStmtIterator* for the
  //! procedure. The user must free the iterator's memory via delete.
  OA::OA_ptr<OA::IRRegionStmtIterator> procBody(OA::ProcHandle h);


  //--------------------------------------------------------
  // Statements: General
  //--------------------------------------------------------

  //! Are return statements allowed
  bool returnStatementsAllowed() { return true; }

  //! Given a statement, return its CFG::IRStmtType
  OA::CFG::IRStmtType getCFGStmtType(OA::StmtHandle h);

  OA::StmtLabel getLabel(OA::StmtHandle h);

  OA::OA_ptr<OA::IRRegionStmtIterator> getFirstInCompound(OA::StmtHandle h);


  //--------------------------------------------------------
  // Loops
  //--------------------------------------------------------
  OA::OA_ptr<OA::IRRegionStmtIterator> loopBody(OA::StmtHandle h);
  OA::StmtHandle loopHeader(OA::StmtHandle h);
  OA::StmtHandle getLoopIncrement(OA::StmtHandle h);
  bool loopIterationsDefinedAtEntry(OA::StmtHandle h);

  //--------------------------------------------------------
  // Structured two-way conditionals
  //--------------------------------------------------------
  OA::OA_ptr<OA::IRRegionStmtIterator> trueBody(OA::StmtHandle h);
  OA::OA_ptr<OA::IRRegionStmtIterator> elseBody(OA::StmtHandle h);
  
  //--------------------------------------------------------
  // Structured multiway conditionals
  //--------------------------------------------------------
  int numMultiCases(OA::StmtHandle h);
  OA::OA_ptr<OA::IRRegionStmtIterator> multiBody(OA::StmtHandle h, 
						 int bodyIndex);
  bool isBreakImplied(OA::StmtHandle h);
  bool isCatchAll(OA::StmtHandle h, int bodyIndex);
  OA::OA_ptr<OA::IRRegionStmtIterator> getMultiCatchall(OA::StmtHandle h);
  OA::ExprHandle getSMultiCondition (OA::StmtHandle h, int bodyIndex);

  //--------------------------------------------------------
  // Unstructured two-way conditionals
  //--------------------------------------------------------
  OA::StmtLabel getTargetLabel(OA::StmtHandle h, int n);
  
  //--------------------------------------------------------
  // Unstructured multi-way conditionals
  //--------------------------------------------------------
  int numUMultiTargets(OA::StmtHandle h);
  OA::StmtLabel getUMultiTargetLabel(OA::StmtHandle h, int targetIndex);
  OA::StmtLabel getUMultiCatchallLabel(OA::StmtHandle h);
  OA::ExprHandle getUMultiCondition(OA::StmtHandle h, int targetIndex);

  //--------------------------------------------------------
  // Special
  //--------------------------------------------------------
  bool parallelWithSuccessor(OA::StmtHandle h) { return false; }
  int numberOfDelaySlots(OA::StmtHandle h);

  //--------------------------------------------------------
  // Symbol Handles
  //--------------------------------------------------------  
  OA::SymHandle getProcSymHandle(OA::ProcHandle h);
  
private:
  OAInterface () { DIAG_Die(DIAG_Unimplemented); }

private:
  binutils::Proc* proc;
  std::set<VMA> branchTargetSet;
};

} // namespace banal

#endif // banal_OAInterface_hpp
