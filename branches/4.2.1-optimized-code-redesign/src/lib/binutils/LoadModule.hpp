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

#ifndef binutils_LM_hpp 
#define binutils_LM_hpp

//************************* System Include Files ****************************

#include <string>
#include <deque>
#include <map>
#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "VMAInterval.hpp"
#include "BinUtils.hpp"

#include <lib/isa/ISATypes.hpp>

//*************************** Forward Declarations **************************

class ISA;

// A map between pairs of addresses in the same procedure and the
// first line of the procedure. This map is built upon reading the
// module. 
// [FIXME: this should be hidden within LoadModule and should be a general addr => procedure map.]
typedef std::map<VMAInterval, suint> VMAToProcMap;

//***************************************************************************
// LoadModule
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
  class DbgFuncSummary;

public:
  enum Type {Executable, SharedLibrary, Unknown};
  
  // Constructor allocates an empty data structure
  LM();
  virtual ~LM();

  // Open: If 'moduleName' is not already open, attempt to do so;
  // return true on success and false otherwise.  If a file is already
  // open return true. (Segs, Procs and Insns are not
  // constructed yet.)
  virtual bool Open(const char* moduleName);

  // Read: If module has not already been read, attempt to do so;
  // return true on success and false otherwise.  If a file has
  // already been read, return true.
  virtual bool Read();

  // GetName: Return name of load module
  const std::string& GetName() const { return name; }

  // GetType:  Return type of load module
  Type GetType() const { return type; }

  // GetTextBeg, GetTextEnd: (Unrelocated) Text begin and end.
  // FIXME: only guaranteed on alpha at present
  VMA GetTextBeg() const { return textBeg; }
  VMA GetTextEnd() const { return textEnd; }

  // FIXME: on platform other than Alpha/Tru64 we need to set these
  void SetTextBeg(VMA x)  { textBeg = x; }
  void SetTextEnd(VMA x)    { textEnd = x; }
  
  VMA GetFirstVMA() const { return firstaddr; }
  void SetFirstVMA(VMA x) { firstaddr = x; }


  // after read in the binary, get the smallest begin VMA and largest end VMA
  // of all the text sections
  void GetTextBegEndVMA(VMA* begVMA, VMA* endVMA);

  // Relocate: 'Relocate' the text section to the supplied text begin
  // address.  All member functions that take VMAs will assume they
  // receive *relocated* values.  A value of 0 unrelocates the module.
  void Relocate(VMA textBegReloc_);
  bool IsRelocated() const;

  // GetNumSegs: Return number of segments/sections
  suint GetNumSegs() const { return sections.size(); }

  // AddSeg: Add a segment/section
  void AddSeg(Seg *section) { sections.push_back(section); }


  // Instructions: All instructions found in text sections may be
  // accessed here, or through other classes (such as a 'Proc').
  //
  // For the sake of generality, all instructions are viewed as
  // (potentially) variable sized instruction packets.  VLIW
  // instructions are 'unpacked' so that each operation is an
  // 'Insn' that may be accessed by the combination of its vma and
  // operation index.  
  //
  // GetMachInsn: Return a pointer to beginning of the instrution
  // bits at virtual memory address 'vma'; NULL if invalid instruction
  // or invalid 'vma'.  Sets 'size' to the size (bytes) of the
  // instruction.
  //
  // GetInsn: Similar to the above except returns an 'Insn',
  // not the bits.
  //
  // AddInsn: Add an instruction to the map
  MachInsn* GetMachInsn(VMA vma, ushort &size) const;
  Insn*     GetInsn(VMA vma, ushort opIndex) const;
  void      AddInsn(VMA vma, ushort opIndex, Insn *insn);

  
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
  // The second version only returns true when all information is
  // found and no error is detected.
  bool GetSourceFileInfo(VMA vma, ushort opIndex,
			 std::string& func, std::string& file, 
			 suint &line) const;

  bool GetSourceFileInfo(VMA begVMA, ushort bOpIndex,
			 VMA endVMA, ushort eOpIndex,
			 std::string& func, std::string& file,
			 suint &begLine, suint &endLine) const;

  bool GetProcFirstLineInfo(VMA vma, ushort opIndex, suint &line);

  DbgFuncSummary* GetDebugFuncSummary() { return &dbgSummary; }

  // It is a little unfortunate to have to make 'isa' global across
  // all intances since it implies that while multiple 'LMs'
  // from the ISA may coexist, only one ISA may be used at a time.
  // The issue is that allowing many types of ISA's to exist at the
  // same time would significantly complicate things and we do not
  // anticiapte such generality being very useful.  For example,
  // having multiple load modules open at the same time is generally
  // not a good idea anyway.
  static ISA* isa; // current ISA

  // Dump contents for inspection
  virtual void Dump(std::ostream& o = std::cerr, const char* pre = "") const;
  virtual void DDump() const;
  virtual void DumpSelf(std::ostream& o = std::cerr, const char* pre = "") const;
  
  friend class LMSegIterator;
  friend class ProcInsnIterator;
  
protected:
  // Should not be used
  LM(const LM& lm) { }
  LM& operator=(const LM& lm) { return *this; }

public:

  // Classes used to represent function summary information obtained
  // from the LM's debugging sections.  This will typically be
  // used in constructing Procs.

  class DbgFuncSummary {
  public:
    class Info {
    public:
      Info() 
	: parent(NULL), parentVMA(0),
	  begVMA(0), endVMA(0), name(""), filenm(""), begLine(0)
        { }
      ~Info() { }
      
      Info* parent;
      VMA  parentVMA;

      VMA begVMA; // begin VMA
      VMA endVMA; // end VMA (at the end of the last insn)
      std::string name, filenm;
      suint begLine;

      std::ostream& dump(std::ostream& os) const;
    };

    typedef VMA                                               key_type;
    typedef Info*                                             mapped_type;
  
    typedef std::map<key_type, mapped_type>                   My_t;
    typedef std::pair<const key_type, mapped_type>            value_type;
    typedef My_t::key_compare                                 key_compare;
    typedef My_t::allocator_type                              allocator_type;
    typedef My_t::reference                                   reference;
    typedef My_t::const_reference                             const_reference;
    typedef My_t::iterator                                    iterator;
    typedef My_t::const_iterator                              const_iterator;
    typedef My_t::size_type                                   size_type;

  public:
    DbgFuncSummary();
    ~DbgFuncSummary();

    void setParentPointers();
    
    // -------------------------------------------------------
    // iterator, find/insert, etc 
    // -------------------------------------------------------
    
    // iterators:
    iterator begin() 
      { return mMap.begin(); }
    const_iterator begin() const 
      { return mMap.begin(); }
    iterator end() 
      { return mMap.end(); }
    const_iterator end() const 
      { return mMap.end(); }
    
    // capacity:
    size_type size() const
      { return mMap.size(); }
    
    // element access:
    mapped_type& operator[](const key_type& x)
      { return mMap[x]; }
    
    // modifiers:
    std::pair<iterator, bool> insert(const value_type& x)
      { return mMap.insert(x); }
    iterator insert(iterator position, const value_type& x)
      { return mMap.insert(position, x); }
    
    void erase(iterator position) 
      { mMap.erase(position); }
    size_type erase(const key_type& x) 
      { return mMap.erase(x); }
    void erase(iterator first, iterator last) 
      { return mMap.erase(first, last); }
    
    void clear();
    
    // mMap operations:
    iterator find(const key_type& x)
      { return mMap.find(x); }
    const_iterator find(const key_type& x) const
      { return mMap.find(x); }
    size_type count(const key_type& x) const
      { return mMap.count(x); }
    
    // -------------------------------------------------------
    // debugging
    // -------------------------------------------------------
    std::ostream& dump(std::ostream& os) const;

  private:
    My_t mMap;
  };

  
private: 
  // Constructing routines: return true on success; false on error
  bool ReadSymbolTables();
  bool ReadSegs();
  bool ReadDebugFunctionSummaryInfo();
  void ClearDebugFunctionSummaryInfo();
  
  // Builds the map from <proc beg addr, proc end addr> pairs to 
  // procedure first line.
  bool buildVMAToProcMap();

  // UnRelocateVMA: Given a relocated VMA, returns a non-relocated version.
  VMA UnRelocateVMA(VMA relocatedVMA) const 
    { return (relocatedVMA + unRelocDelta); }
  
  // Comparison routines for QuickSort.
  static int SymCmpByVMAFunc(const void* s1, const void* s2);

  // Callback for bfd_elf_forall_dbg_funcinfo
  static int bfd_DbgFuncinfoCallback(void* callback_obj, void* parent, void* funcinfo);

  // Dump helper routines
  void DumpModuleInfo(std::ostream& o = std::cerr, const char* pre = "") const;
  void DumpSymTab(std::ostream& o = std::cerr, const char* pre = "") const;
  
  // Virtual memory address to Insn* map: Note that 'VMA' is
  // not necessarily the true vma value; rather, it is the address of
  // the individual operation (ISA::ConvertVMAToOpVMA).
  typedef std::map<VMA, Insn*, lt_VMA> VMAToInsnMap;
  
  // Seg sequence: 'deque' supports random access iterators (and
  // is thus sortable with std::sort) and constant time insertion/deletion at
  // beginning/end.
  typedef std::deque<Seg*> SegSeq;
  typedef std::deque<Seg*>::iterator SegSeqIt;
  typedef std::deque<Seg*>::const_iterator SegSeqItC;
  
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

  // A map of virtual memory addresses to Insn*
  VMAToInsnMap vmaToInsnMap;  // owns all Insn*
  VMAToProcMap vmaToProcMap; // CC

  // symbolic info used in building procedures
  DbgFuncSummary dbgSummary; 
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
  Exe(); // set type to executable
  virtual ~Exe();

  // See LM::Open comments
  virtual bool Open(const char* moduleName);
  
  VMA GetStartVMA() const { return startVMA; }
  
  virtual void Dump(std::ostream& o = std::cerr, const char* pre = "") const;
  virtual void DDump() const; 
  virtual void DumpSelf(std::ostream& o = std::cerr, const char* pre = "") const;

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
  LM::SegSeqItC it;
};

} // namespace binutils

//***************************************************************************

#endif // binutils_LM_hpp
