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

#include "dbg_LM.hpp"
#include "VMAInterval.hpp"
#include "BinUtils.hpp"

#include <lib/isa/ISATypes.hpp>

#include <lib/support/Exception.hpp>
#include <lib/support/SrcFile.hpp>

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
  enum Type { Executable, SharedLibrary, Unknown };

  // VMAInterval to Proc map.
  typedef VMAIntervalMap<Proc*> VMAToProcMap;
  
  // Virtual memory address to Insn* map.
  typedef std::map<VMA, Insn*> VMAToInsnMap;
  
  // Seg sequence: 'deque' supports random access iterators (and
  // is thus sortable with std::sort) and constant time insertion/deletion at
  // beginning/end.
  typedef std::deque<Seg*> SegSeq;
  
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

  // Open: If 'moduleName' is not already open, attempt to do so;
  // throw an exception on error.  If a file is already, do nothing.
  // (Segs, Procs and Insns are not constructed yet.)
  virtual void 
  Open(const char* moduleName);

  // Read: If module has not already been read, attempt to do so;
  // return an exception on error.  If a file has already been read do
  // nothing.
  virtual void 
  Read();


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  // GetName: Return name of load module
  const std::string& 
  GetName() const { return name; }

  // GetType:  Return type of load module
  Type 
  GetType() const { return type; }

  // GetTextBeg, GetTextEnd: (Unrelocated) Text begin and end.
  // FIXME: only guaranteed on alpha at present
  VMA 
  GetTextBeg() const { return textBeg; }

  VMA 
  GetTextEnd() const { return textEnd; }

  // FIXME: on platform other than Alpha/Tru64 we need to set these
  void 
  SetTextBeg(VMA x)  { textBeg = x; }

  void 
  SetTextEnd(VMA x)  { textEnd = x; }
  
  VMA 
  GetFirstVMA() const { return firstaddr; }

  void 
  SetFirstVMA(VMA x) { firstaddr = x; }


  // after reading the binary, get the smallest begin VMA and largest end VMA
  // of all the text sections
  void 
  GetTextBegEndVMA(VMA* begVMA, VMA* endVMA);

  // Relocate: 'Relocate' the text section to the supplied text begin
  // address.  All member functions that take VMAs will assume they
  // receive *relocated* values.  A value of 0 unrelocates the module.
  void 
  Relocate(VMA textBegReloc_);

  bool 
  IsRelocated() const;

  // GetNumSegs: Return number of segments/sections
  uint 
  GetNumSegs() const { return sections.size(); }

  // AddSeg: Add a segment/section
  void 
  AddSeg(Seg* section) { sections.push_back(section); }


  // -------------------------------------------------------
  // Procedures: All procedures found in text sections may be
  // accessed here.
  // -------------------------------------------------------
  Proc* 
  findProc(VMA vma) const;

  void  
  insertProc(VMAInterval vmaint, Proc* proc);  
  
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
  
  Insn*
  findInsn(VMA vma, ushort opIndex) const;

  void
  insertInsn(VMA vma, ushort opIndex, Insn* insn);

  
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
		    std::string& file, SrcFile::ln& line) const;

  bool 
  GetSourceFileInfo(VMA begVMA, ushort bOpIndex,
		    VMA endVMA, ushort eOpIndex,
		    std::string& func, std::string& file,
		    SrcFile::ln& begLine, SrcFile::ln& endLine,
		    unsigned flags = 1) const;

  bool 
  GetProcFirstLineInfo(VMA vma, ushort opIndex, SrcFile::ln& line) const;

  
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
    DUMP_Flg_Insn        = 0x00000010, // print insns
    DUMP_Flg_Insn_decode = 0x00000020, // print insns (decoded)
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
  
  friend class LMSegIterator;
  friend class ProcInsnIterator;
  
protected:
  // Should not be used
  LM(const LM& lm) { }
  LM& operator=(const LM& lm) { return *this; }

private: 
  // Constructing routines: return true on success; false on error
  bool 
  ReadSymbolTables();

  bool 
  ReadSegs();
  
  // UnRelocateVMA: Given a relocated VMA, returns a non-relocated version.
  VMA 
  UnRelocateVMA(VMA relocatedVMA) const 
    { return (relocatedVMA + unRelocDelta); }
  
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
  GetDebugInfo() { return &mDbgInfo; }
    
protected:
  LMImpl* impl; 

private:
  std::string name;
  Type  type;
  VMA   textBeg, textEnd; // text begin and end
  VMA   firstaddr;        // shared library load address begin
  VMA   textBegReloc;     // relocated text begin
  VMASigned unRelocDelta; // offset to unrelocate relocated VMAs
    
  SegSeq sections; // A list of sections

  // A map of VMAs to Proc* and Insn*.  
  //   1. 'vmaToProcMap' is indexed by an interval [a b) where a is
  // the begin VMA of this procedure and b is the begin VMA of the
  // following procedure (or the end of the section if there is no
  // following procedure).
  //   2. For 'vmaToInsnMap', note that 'VMA' is not necessarily the
  // true vma value; rather, it is the address of the individual
  // operation (ISA::ConvertVMAToOpVMA).
  VMAToProcMap vmaToProcMap;
  VMAToInsnMap vmaToInsnMap; // owns all Insn*

  // symbolic info used in building procedures
  binutils::dbg::LM mDbgInfo;
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
  Exe(); // set type to executable
  virtual ~Exe();

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  // See LM::Open comments
  virtual void Open(const char* moduleName);
  
  VMA GetStartVMA() const { return startVMA; }

  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  virtual void dump(std::ostream& o = std::cerr, 
		    int flags = DUMP_Short, const char* pre = "") const;

  virtual void dumpme(std::ostream& o = std::cerr, const char* pre = "") const;

private:
  // Should not be used
  Exe(const Exe& e) { }
  Exe& operator=(const Exe& e) { return *this; }
  
protected:
private:
  VMA startVMA;
};

} // namespace binutils


//***************************************************************************
// LMSegIterator
//***************************************************************************

namespace binutils {

class Seg;

// --------------------------------------------------------------------------
// 'LMSegIterator': iterator over the sections in a 'LM'
// --------------------------------------------------------------------------

class LMSegIterator {
public:   
  LMSegIterator(const LM& _lm);
  ~LMSegIterator();

  // Returns the current object or NULL
  Seg* Current() const {
    if (it != lm.sections.end()) { return *it; }
    else { return NULL; }
  }
  
  void operator++()    { ++it; } // prefix increment
  void operator++(int) { it++; } // postfix increment

  bool IsValid() const { return it != lm.sections.end(); } 
  bool IsEmpty() const { return it == lm.sections.end(); }

  // Reset and prepare for iteration again
  void Reset() { it = lm.sections.begin(); }

private:
  // Should not be used
  LMSegIterator();
  LMSegIterator(const LMSegIterator& i);
  LMSegIterator& operator=(const LMSegIterator& i)
    { return *this; }

protected:
private:
  const LM& lm;
  LM::SegSeq::const_iterator it;
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
