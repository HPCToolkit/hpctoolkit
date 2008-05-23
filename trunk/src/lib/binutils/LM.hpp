// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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

#ifndef binutils_LM_hpp 
#define binutils_LM_hpp

//************************* System Include Files ****************************

#include <string>
#include <deque>
#include <map>
#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h>
#include <include/gnu_bfd.h>

#include "dbg_LM.hpp"
#include "VMAInterval.hpp"
#include "BinUtils.hpp"

#include <lib/isa/ISATypes.hpp>
#include <lib/isa/ISA.hpp>

#include <lib/support/Exception.hpp>
#include <lib/support/SrcFile.hpp>
#include <lib/support/RealPathMgr.hpp>

//*************************** Forward Declarations **************************

class ISA;

//***************************************************************************
// LM (LoadModule)
//***************************************************************************

namespace binutils {

class Seg;
class Proc;
class Insn;
class LMImpl; 

// --------------------------------------------------------------------------
// 'LM' represents a load module, a binary loaded into memory
//
//
// Note: Most references to VMA (virtual memory address) could be
// replaced with 'PC' (program counter) or IP (instruction pointer).
// --------------------------------------------------------------------------

class LM {
public:
  enum Type { TypeNULL = 0, TypeExe, TypeDSO };

  // Read flags: forms an inverse hierarchy where a smaller scope
  // implies all the larger scopes.  E.g. ReadFlg_Insn implies
  // ReadFlg_Proc and ReadFlg_Seg.
  enum ReadFlg { 
    ReadFlg_NULL = 0,
    ReadFlg_Seg  = 0x0001, // always read: permits source code lookup
    ReadFlg_Proc = 0x0011,
    ReadFlg_Insn = 0x0111,

    ReadFlg_ALL  = ReadFlg_Seg | ReadFlg_Proc | ReadFlg_Insn
  };

  typedef VMAIntervalMap<Seg*>  SegMap;
  typedef VMAIntervalMap<Proc*> ProcMap;
  typedef std::map<VMA, Insn*>  InsnMap;
  
public:
  // -------------------------------------------------------  
  // Constructor/Destructor
  // -------------------------------------------------------

  // Constructor allocates an empty data structure
  LM();
  virtual ~LM();

  // -------------------------------------------------------  
  // open/read (cf. istreams)
  // -------------------------------------------------------

  // open: If 'moduleName' is not already open, attempt to do so;
  // throw an exception on error.  If a file is already, do nothing.
  // (Segs, Procs and Insns are not constructed yet.)
  virtual void open(const char* filenm);

  // read: If module has not already been read, attempt to do so;
  // return an exception on error.  If a file has already been read do
  // nothing.
  virtual void read(ReadFlg readflg/* = ReadFlg_Seg*/);


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  // name: Return name of load module
  const std::string& name() const { return m_name; }

  // type:  Return type of load module
  Type type() const { return m_type; }

  ReadFlg readFlags() { return m_readFlags; }

  // textBeg, textEnd: (Unrelocated) Text begin and end.
  // FIXME: only guaranteed on alpha at present
  VMA textBeg() const { return m_txtBeg; }
  VMA textEnd() const { return m_txtEnd; }

  // FIXME: should be automatically set... (cf. Alpha/Tru64)
  void textBeg(VMA x)  { m_txtBeg = x; }
  void textEnd(VMA x)  { m_txtEnd = x; }
  
  VMA  firstVMA() const { return m_begVMA; }
  void firstVMA(VMA x) { m_begVMA = x; }


  // after reading the binary, get the smallest begin VMA and largest end VMA
  // of all the text m_sections
  void textBegEndVMA(VMA* begVMA, VMA* endVMA);

  // relocate: 'Relocate' the text section to the supplied text begin
  // address.  All member functions that take VMAs will assume they
  // receive *relocated* values.  A value of 0 unrelocates the module.
  void relocate(VMA textBegReloc);

  bool isRelocated() const {
    return (m_textBegReloc != 0);
  }

  // doUnrelocate: is unrelocation necessary?
  bool doUnrelocate(VMA loadAddr) const {
    return ((type() == TypeDSO) && (textBeg() < loadAddr));
  }

  // -------------------------------------------------------
  // Segments: 
  // -------------------------------------------------------

  SegMap&       segs()       { return m_segMap; }
  const SegMap& segs() const { return m_segMap; }

  Seg* 
  findSeg(VMA vma) const
  {
    VMA vma_ur = unrelocate(vma);
    VMAInterval ival_ur(vma_ur, vma_ur + 1); // size must be > 0
    SegMap::const_iterator it = m_segMap.find(ival_ur);
    Seg* seg = (it != m_segMap.end()) ? it->second : NULL;
    return seg;
  }

  // NOTE: does not check for duplicates
  void 
  insertSeg(VMAInterval ival, Seg* seg) 
  {
    VMAInterval ival_ur(unrelocate(ival.beg()), unrelocate(ival.end()));
    m_segMap.insert(SegMap::value_type(ival_ur, seg));
  }

  uint numSegs() const { return m_segMap.size(); }


  // -------------------------------------------------------
  // Procedures: All procedures may be accessed here.
  // -------------------------------------------------------

  ProcMap&       procs()       { return m_procMap; }
  const ProcMap& procs() const { return m_procMap; }

  Proc* 
  findProc(VMA vma) const
  {
    VMA vma_ur = unrelocate(vma);
    VMAInterval ival_ur(vma_ur, vma_ur + 1); // size must be > 0
    ProcMap::const_iterator it = m_procMap.find(ival_ur);
    Proc* proc = (it != m_procMap.end()) ? it->second : NULL;
    return proc;
  }

  // NOTE: does not check for duplicates
  void 
  insertProc(VMAInterval ival, Proc* proc) 
  {
    VMAInterval ival_ur(unrelocate(ival.beg()), unrelocate(ival.end()));
    m_procMap.insert(ProcMap::value_type(ival_ur, proc));
  }

  
  // -------------------------------------------------------
  // Instructions: All instructions found in text sections may be
  // accessed here, or through other classes (such as a 'Proc').
  //
  // For the sake of generality, all instructions are viewed as
  // (potentially) variable sized instruction packets.  VLIW
  // instructions are 'unpacked' so that each operation is an
  // 'Insn' that may be accessed by the combination of its vma and
  // operation index.  
  //
  // findMachInsn: Return a pointer to beginning of the instrution
  // bits at virtual memory address 'vma'; NULL if invalid instruction
  // or invalid 'vma'.  Sets 'size' to the size (bytes) of the
  // instruction.
  //
  // findInsn: Similar to the above except returns an 'Insn',
  // not the bits.
  //
  // insertInsn: Add an instruction to the map
  // -------------------------------------------------------
  MachInsn*
  findMachInsn(VMA vma, ushort &size) const;
  
  InsnMap&       insns()       { return m_insnMap; }
  const InsnMap& insns() const { return m_insnMap; }

  Insn*
  findInsn(VMA vma, ushort opIndex) const
  {
    VMA vma_ur = unrelocate(vma);
    VMA opvma = isa->ConvertVMAToOpVMA(vma_ur, opIndex);
    
    InsnMap::const_iterator it = m_insnMap.find(opvma);
    Insn* insn = (it != m_insnMap.end()) ? it->second : NULL;
    return insn;
  }

  // NOTE: does not check for duplicates
  void
  insertInsn(VMA vma, ushort opIndex, Insn* insn)
  {
    VMA vma_ur = unrelocate(vma);
    VMA opvma = isa->ConvertVMAToOpVMA(vma_ur, opIndex);
    m_insnMap.insert(InsnMap::value_type(opvma, insn));
  }

  
  // -------------------------------------------------------
  // GetSourceFileInfo: If possible, find the source file, function
  // name and line number that corresponds to the operation at
  // 'vma + opIndex'.  If it is possible to find all information without
  // errors, return true; otherwise false.
  //
  // For convenience, another version is provided that takes a
  // 'begVMA + begOpIndex' and 'endVMA + endOpIndex'.  Note that the
  // following condition is enforced:
  //   'begVMA + opIndex' <= 'endVMA + opIndex'.
  // All virtual memory address vma values should point to the
  // *beginning* of instructions (not at the end or middle).
  //
  // Note that this second version attempts to select the best
  // function, file and line information from both individual calls to
  // the first version, correcting as best it can for errors.  In
  // particular:
  //   - If 'file' is NULL, both 'begLine' and 'endLine' will be 0.
  //   - If 'file' is not NULL, *both* line numbers may or may not be non-0;
  //     never will only one line number be 0.
  //   - It is an error for either the file or function to be
  //     different accross the individual calls.  In this case
  //     information from 'begVMA' is used.
  // 
  // If 'flags' is set to 1, then beg/end line swapping is performed.
  // 
  // The second version only returns true when all information is
  // found and no error is detected.
  // -------------------------------------------------------
  bool 
  GetSourceFileInfo(VMA vma, ushort opIndex,
		    std::string& func, 
		    std::string& file, SrcFile::ln& line) /*const*/;

  bool 
  GetSourceFileInfo(VMA begVMA, ushort bOpIndex,
		    VMA endVMA, ushort eOpIndex,
		    std::string& func, std::string& file,
		    SrcFile::ln& begLine, SrcFile::ln& endLine,
		    unsigned flags = 1) /*const*/;

  bool 
  GetProcFirstLineInfo(VMA vma, ushort opIndex, SrcFile::ln& line) const;

  bool realpath(std::string& fnm) { m_realpath_mgr.realpath(fnm); }


  // -------------------------------------------------------
  // BFD details
  // -------------------------------------------------------
  bfd*      abfd()        const { return m_bfd; }
  asymbol** bfdSymTab()   const { return m_bfdSymTabSort; }
  uint      bfdSymTabSz() const { return m_bfdSymTabSz; }

  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  enum DumpTy { 
    // Shorthand notation
    DUMP_Short           = 0x00000000,
    DUMP_Mid             = 0x00000110,
    DUMP_Mid_decode      = 0x00000130,
    DUMP_Long            = 0x11111111,
    DUMP_Long_decode     = 0x11111131,
    
    // Shorthand meanings
    DUMP_Flg_SymTab      = 0x00000001, // print sym-tab
    DUMP_Flg_Insn_ty     = 0x00000010, // print insn (type)
    DUMP_Flg_Insn_decode = 0x00000020, // print insn (decoded)
    DUMP_Flg_Sym         = 0x00000100  // print symbolic info
  };

  std::string 
  toString(int flags = DUMP_Short, const char* pre = "") const;

  virtual void 
  dump(std::ostream& o = std::cerr, int flags = DUMP_Short, 
       const char* pre = "") const;

  void 
  ddump() const;
  
  // dump helpers
  virtual void 
  dumpme(std::ostream& o = std::cerr, const char* pre = "") const;

  virtual void 
  dumpProcMap(std::ostream& o = std::cerr, unsigned flag = 0, const char* pre = "") const;

  void 
  ddumpProcMap(unsigned flag) const;

  // -------------------------------------------------------
  // It is a little unfortunate to have to make 'isa' global across
  // all intances since it implies that while multiple 'LMs'
  // from the ISA may coexist, only one ISA may be used at a time.
  // The issue is that allowing many types of ISA's to exist at the
  // same time would significantly complicate things and we do not
  // anticiapte such generality being very useful.  For example,
  // having multiple load modules open at the same time is generally
  // not a good idea anyway.
  // -------------------------------------------------------
  static ISA* isa; // current ISA
  
  friend class ProcInsnIterator;
  
protected:
  // Should not be used
  LM(const LM& lm) { }
  LM& operator=(const LM& lm) { return *this; }

private: 
  // Constructing routines: return true on success; false on error
  void
  readSymbolTables();

  void
  readSegs();
  
  // unrelocate: Given a relocated VMA, returns a non-relocated version.
  VMA unrelocate(VMA relocVMA) const { return (relocVMA + m_unrelocDelta); }
  
  // Comparison routines for QuickSort.
  static int 
  SymCmpByVMAFunc(const void* s1, const void* s2);

  // Dump helper routines
  void 
  DumpModuleInfo(std::ostream& o = std::cerr, const char* pre = "") const;

  void 
  DumpSymTab(std::ostream& o = std::cerr, const char* pre = "") const;

  friend class TextSeg; // for TextSeg::Create_InitializeProcs();

  binutils::dbg::LM* 
  GetDebugInfo() { 
    return &m_dbgInfo; 
  }
    
private:
  std::string m_name;

  Type    m_type;
  ReadFlg m_readFlags;

  VMA  m_txtBeg, m_txtEnd; // text begin and end
  VMA  m_begVMA;           // shared library load address begin

  VMA       m_textBegReloc; // relocated text begin
  VMASigned m_unrelocDelta; // offset to unrelocate relocated VMAs

  // - m_segMapm, m_procMap: Segments and procedures are indexed by an
  //   interval [begVMA endVMA)
  //
  // - m_insnMap: note that 'VMA' is not necessarily the true vma
  //   value; rather, it is the address of the individual operation
  //   (ISA::ConvertVMAToOpVMA).
  SegMap  m_segMap;  // owns all Seg*
  ProcMap m_procMap;
  InsnMap m_insnMap; // owns all Insn*

  // symbolic info used in building procedures
  binutils::dbg::LM m_dbgInfo;

  bfd*      m_bfd;           // BFD of this module.
  asymbol** m_bfdSymTab;     // Unmodified BFD symbol table
  asymbol** m_bfdSymTabSort; // Sorted BFD symbol table
  uint      m_bfdSymTabSz;   // Number of syms in table.

  RealPathMgr m_realpath_mgr;
};

} // namespace binutils


//***************************************************************************
// Executable
//***************************************************************************

namespace binutils {

// --------------------------------------------------------------------------
// 'Executable' represents an executable binary
// --------------------------------------------------------------------------

class Exe : public LM {
public:
  // -------------------------------------------------------  
  // Constructor/Destructor
  // -------------------------------------------------------
  Exe();
  virtual ~Exe();

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  // See LM::Open comments
  virtual void open(const char* filenm);
  
  VMA GetStartVMA() const { return m_startVMA; }

  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  virtual void dump(std::ostream& o = std::cerr, 
		    int flags = DUMP_Short, const char* pre = "") const;

  virtual void dumpme(std::ostream& o = std::cerr, const char* pre = "") const;

private:
  // Should not be used
  Exe(const Exe& e); // { }
  Exe& operator=(const Exe& e); // { return *this; }
  
protected:
private:
  VMA m_startVMA;
};

} // namespace binutils


//***************************************************************************
// Exception
//***************************************************************************

#define BINUTILS_Throw(streamArgs) DIAG_ThrowX(binutils::Exception, streamArgs)

namespace binutils {

class Exception : public Diagnostics::Exception {
public:
  Exception(const std::string x, const char* filenm = NULL, uint lineno = 0)
    : Diagnostics::Exception(x, filenm, lineno)
  { }
  
  virtual std::string message() const { 
    return "[binutils]: " + what();
  }

private:
};

} // namespace binutils


//***************************************************************************

#endif // binutils_LM_hpp
