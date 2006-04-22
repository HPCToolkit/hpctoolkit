// $Id$
// -*-C++-*-
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
//    LoadModule.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef LoadModule_H 
#define LoadModule_H

//************************* System Include Files ****************************

#include <deque>
#include <map>
#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "BinUtils.h"
#include <lib/ISA/ISATypes.h>
#include <lib/support/String.h>

//*************************** Forward Declarations **************************

class Section;
class Procedure;
class Instruction;

class ISA;

// FIXME: It is a little unfortunate to have to make 'isa' global.
// Though multiple 'LoadModules' from the ISA may coexist, only one
// ISA may be used at a time.  Allowing many types of ISA's to exist
// at the same time would significantly complicate things and we do
// not anticiapte such generality being very useful.
extern ISA* isa; // current ISA


/// A map between pairs of addresses in the same procedure and the first line of the procedure. This map is built upon reading the module.
using namespace std;

typedef pair< Addr, Addr > AddrPair;

struct PairAddrLt {
  bool operator() (const AddrPair pair1, const AddrPair pair2) const {
    return (pair1.second < pair2.first);
  }
};
typedef map< pair<Addr,Addr>, suint, PairAddrLt>  AddrToProcedureMap;
typedef AddrToProcedureMap::iterator AddrToProcedureMapIt;
typedef AddrToProcedureMap::value_type AddrToProcedureMapVal;

//***************************************************************************
// LoadModule
//***************************************************************************

// 'LoadModule' represents a binary loaded into memory

// Note: everywhere 'PC' (program counter) is used, read VMA (virtual
// memory address) or instruction pointer.

class LoadModuleImpl; 

class LoadModule {
public:
  enum Type {Executable, SharedLibrary, Unknown};
  
  // Constructor allocates an empty data structure
  LoadModule();
  virtual ~LoadModule();

  // Open: If 'moduleName' is not already open, attempt to do so;
  // return true on success and false otherwise.  If a file is already
  // open return true. (Sections, Procedures and Instructions are not
  // constructed yet.)
  virtual bool Open(const char* moduleName);

  // Read: If module has not already been read, attempt to do so;
  // return true on success and false otherwise.  If a file has
  // already been read, return true.
  virtual bool Read();

  // GetName: Return name of load module
  String GetName() const { return name; }

  // GetType:  Return type of load module
  Type GetType() const { return type; }

  // GetTextStart, GetTextEnd: (Unrelocated) Text start and end.
  // FIXME: only guaranteed on alpha at present
  Addr GetTextStart() const { return textStart; }
  Addr GetTextEnd() const { return textEnd; }   
 
  // on platform other than alpha we need to set textStart and textEnd
  void SetTextStart(Addr pv)  {textStart=pv; }
  void SetTextEnd(Addr pv)    {textEnd=pv; }   

  Addr GetFirstAddr() const {return firstaddr ; }
  void SetFirstAddr(Addr pv) {firstaddr = pv ; }


  // after read in the binary, get the smallest start PC and largest end PC
  // of all the text sections
  void GetTextStartEndPC(Addr* startpc,Addr* endpc);

  // Relocate: 'Relocate' the text section to the supplied text start
  // address.  All member functions that take PCs will assume they
  // receive *relocated* values.  A value of 0 unrelocates the module.
  void Relocate(Addr textStartReloc_);
  bool IsRelocated() const;

  // GetNumSections: Return number of sections
  suint GetNumSections() const { return sections.size(); }

  // AddSection: Add a section
  void AddSection(Section *section) { sections.push_back(section); }


  // Instructions: All instructions found in text sections may be
  // accessed here, or through other classes (such as a 'Procedure').
  //
  // For the sake of generality, all instructions are viewed as
  // (potentially) variable sized instruction packets.  VLIW
  // instructions are 'unpacked' so that each operation is an
  // 'Instruction' that may be accessed by the combination of its pc and
  // operation index.  
  //
  // GetMachInst: Return a pointer to beginning of the instrution
  // bits at virtual memory address 'pc'; NULL if invalid instruction
  // or invalid 'pc'.  Sets 'size' to the size (bytes) of the
  // instruction.
  //
  // GetInst: Similar to the above except returns an 'Instruction',
  // not the bits.
  //
  // AddInst: Add an instruction to the map
  MachInst*    GetMachInst(Addr pc, ushort &size) const;
  Instruction* GetInst(Addr pc, ushort opIndex) const;
  void AddInst(Addr pc, ushort opIndex, Instruction *inst);

  
  // GetSourceFileInfo: If possible, find the source file, function
  // name and line number that corresponds to the operation at
  // 'pc + opIndex'.  If it is possible to find all information without
  // errors, return true; otherwise false.
  //
  // For convenience, another version is provided that takes a
  // 'startPC + startOpIndex' and 'endPC + endOpIndex'.  Note that the
  // following condition is enforced:
  //   'startPC + opIndex' <= 'endPC + opIndex'.
  // All virtual memory address pc values should point to the
  // *beginning* of instructions (not at the end or middle).
  //
  // Note that this second version attempts to select the best
  // function, file and line information from both individual calls to
  // the first version, correcting as best it can for errors.  In
  // particular:
  //   - If 'file' is NULL, both 'startLine' and 'endLine' will be 0.
  //   - If 'file' is not NULL, *both* line numbers may or may not be non-0;
  //     never will only one line number be 0.
  //   - It is an error for either the file or function to be
  //     different accross the individual calls.  In this case
  //     information from 'startPC' is used.
  // The second version only returns true when all information is
  // found and no error is detected.
  bool GetSourceFileInfo(Addr pc, ushort opIndex,
			 String &func, String &file, suint &line) const;
  bool GetSourceFileInfo(Addr startPC, ushort sOpIndex,
			 Addr endPC, ushort eOpIndex,
			 String &func, String &file,
			 suint &startLine, suint &endLine) const;

  bool GetProcedureFirstLineInfo(Addr pc, ushort opIndex, suint &line);
 
  // Dump contents for inspection
  virtual void Dump(std::ostream& o = std::cerr, const char* pre = "") const;
  virtual void DDump() const;
  virtual void DumpSelf(std::ostream& o = std::cerr, const char* pre = "") const;
  
  friend class LoadModuleSectionIterator;
  friend class ProcedureInstructionIterator;
  
protected:
  // Should not be used
  LoadModule(const LoadModule& lm) { }
  LoadModule& operator=(const LoadModule& lm) { return *this; }

private: 
  // Constructing routines: return true on success; false on error
  bool ReadSymbolTables();
  bool ReadSections();
 
  // Builds the map from <proc start addr, proc end addr> pairs to 
  // procedure first line.
  bool buildAddrToProcedureMap();

  // UnRelocatePC: Given a relocated PC, returns a non-relocated version.
  Addr UnRelocatePC(Addr relocatedPC) const 
    { return (relocatedPC + unRelocDelta); }
  
  // Comparison routines for QuickSort.
  static int SymCmpByVMAFunc(const void* s1, const void* s2);

  // Dump helper routines
  void DumpModuleInfo(std::ostream& o = std::cerr, const char* pre = "") const;
  void DumpSymTab(std::ostream& o = std::cerr, const char* pre = "") const;
  
  // Virtual memory address to Instruction* map: Note that 'Addr' is
  // not necessarily the true pc value; rather, it is the address of
  // the individual operation (ISA::ConvertPCToOpPC).
  typedef std::map<Addr, Instruction*, lt_Addr>           AddrToInstMap;
  typedef std::map<Addr, Instruction*, lt_Addr>::iterator AddrToInstMapIt;
  typedef std::map<Addr, Instruction*, lt_Addr>::const_iterator
    AddrToInstMapItC;  
  typedef std::map<Addr, Instruction*>::value_type        AddrToInstMapVal;
  
  // Section sequence: 'deque' supports random access iterators (and
  // is thus sortable with std::sort) and constant time insertion/deletion at
  // beginning/end.
  typedef std::deque<Section*>           SectionSeq;
  typedef std::deque<Section*>::iterator SectionSeqIt;
  typedef std::deque<Section*>::const_iterator SectionSeqItC;
  
protected:
  LoadModuleImpl* impl; 

private: 
  String name;
  Type   type;
  Addr   textStart, textEnd; // text begin and end 
  Addr    firstaddr ;        // shared library load address begin
  Addr   textStartReloc;     // relocated text start
  AddrSigned unRelocDelta;   // offset to unrelocate relocated PCs
    
  SectionSeq sections; // A list of sections

  // A map of virtual memory addresses to Instruction*
  AddrToInstMap addrToInstMap;  // owns all Instruction*
  AddrToProcedureMap addrToProcedureMap; // CC
};

//***************************************************************************
// Executable
//***************************************************************************

// 'Executable' represents an executable binary

class Executable : public LoadModule {
public:
  Executable(); // set type to executable
  virtual ~Executable();

  // See LoadModule::Open comments
  virtual bool Open(const char* moduleName);
  
  Addr GetStartAddr() const { return startAddr; }
  
  virtual void Dump(std::ostream& o = std::cerr, const char* pre = "") const;
  virtual void DDump() const; 
  virtual void DumpSelf(std::ostream& o = std::cerr, const char* pre = "") const;

private:
  // Should not be used
  Executable(const Executable& e) { }
  Executable& operator=(const Executable& e) { return *this; }
  
protected:
private:
  Addr startAddr;
};

//***************************************************************************
// LoadModuleSectionIterator
//***************************************************************************

// 'LoadModuleSectionIterator': iterator over the sections in a 'LoadModule'

class LoadModuleSectionIterator {
public:   
  LoadModuleSectionIterator(const LoadModule& _lm);
  ~LoadModuleSectionIterator();

  // Returns the current object or NULL
  Section* Current() const {
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
  LoadModuleSectionIterator();
  LoadModuleSectionIterator(const LoadModuleSectionIterator& i);
  LoadModuleSectionIterator& operator=(const LoadModuleSectionIterator& i)
    { return *this; }

protected:
private:
  const LoadModule& lm;
  LoadModule::SectionSeqItC it;
};

//***************************************************************************

#endif 
