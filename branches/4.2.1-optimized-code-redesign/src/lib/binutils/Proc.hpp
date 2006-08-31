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

#ifndef binutils_Proc_hpp 
#define binutils_Proc_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <string>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "LM.hpp"
#include "Seg.hpp"

#include <lib/isa/ISATypes.hpp>

//*************************** Forward Declarations **************************

class TextSeg;

//***************************************************************************
// Proc
//***************************************************************************

namespace binutils {

// --------------------------------------------------------------------------
// 'Proc' represents a procedure in the 'TextSeg' of a 'LM'
// --------------------------------------------------------------------------

class Proc {
public:
  enum Type { Local, Weak, Global, Unknown };
  
public:
  // -------------------------------------------------------  
  // Constructor/Destructor
  // -------------------------------------------------------
  Proc(TextSeg* _sec, std::string& _name, std::string& _linkname, 
       Type t, VMA _begVMA, VMA _endVMA, suint _size);
  virtual ~Proc();


  // -------------------------------------------------------
  // Basic data
  // -------------------------------------------------------

  TextSeg* GetTextSeg() const { return sec; }
  LM*  GetLM()  const { return sec->GetLM(); }

  // Returns the name as determined by debugging information; if this
  // is unavailable returns the name found in the symbol table.  (Note
  // that no demangling is performed.)
  const std::string& GetName()     const { return name; }
  std::string&       GetName()           { return name; }

  // Returns the name as found in the symbol table
  const std::string& GetLinkName() const { return linkname; }

  // Return type of procedure
  Type  GetType() const { return type; }

  // Return the begin and end virtual memory address of a procedure:
  // [beg, end].  Note that the end address points to the beginning of
  // the last instruction which is different than the convention used
  // for 'Seg'.
  VMA  GetBegVMA() const { return begVMA; }
  VMA  GetEndVMA() const { return endVMA; }
  void SetEndVMA(VMA _endVMA) { endVMA = _endVMA; }

  // Return size, which is (endVMA - startVMA) + sizeof(last instruction)
  suint GetSize()      const { return size; }
  void SetSize(suint _size)  { size = _size; }
  
  // -------------------------------------------------------
  // Symbolic information: availability depends upon debugging information
  // -------------------------------------------------------

  bool hasSymbolic() { return begLine != 0; }

  const std::string& GetFilename() const { return filenm; }
  std::string&       GetFilename()       { return filenm; }

  suint   GetBegLine()      const { return begLine; }
  suint&  GetBegLine()            { return begLine; }

  Proc*  parent() const { return mParent; }
  Proc*& parent()       { return mParent; }

  // -------------------------------------------------------
  //
  // -------------------------------------------------------

  // Return true if virtual memory address 'vma' is within the procedure
  // WARNING: vma must be unrelocated
  bool  IsIn(VMA vma)  const { return (begVMA <= vma && vma <= endVMA); }

  // Return the unique number assigned to this procedure
  suint GetId()        const { return id; }

  // Return the number of instructions in the procedure (FIXME: never computed)
  suint GetNumInsns()  const { return numInsns; }

  // Return the first and last instruction in the procedure
  Insn* GetFirstInsn() const { return findInsn(begVMA, 0); }
  Insn* GetLastInsn() const;
  
  // -------------------------------------------------------
  // Convenient wrappers for the 'LM' versions of the same.
  // -------------------------------------------------------

  MachInsn* findMachInsn(VMA vma, ushort &sz) const {
    return sec->GetLM()->findMachInsn(vma, sz);
  }
  Insn* findInsn(VMA vma, ushort opIndex) const {
    return sec->GetLM()->findInsn(vma, opIndex);
  }
  bool GetSourceFileInfo(VMA vma, ushort opIndex,
			 std::string& func, std::string& file, 
			 suint& line) const {
    return sec->GetLM()->GetSourceFileInfo(vma, opIndex,
					   func, file, line);
  }
  bool GetSourceFileInfo(VMA begVMA, ushort bOpIndex,
			 VMA endVMA, ushort eOpIndex,
			 std::string& func, std::string& file,
			 suint& begLine, suint& endLine) const {
    return sec->GetLM()->GetSourceFileInfo(begVMA, bOpIndex,
					   endVMA, eOpIndex, 
					   func, file, 
					   begLine, endLine);
  }
  
  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  // Current meaning of 'flags'
  //   0 : short dump (without instructions)
  //   1 : full dump
  
  virtual std::string toString(unsigned flags = 0) const;

  virtual void dump(std::ostream& o = std::cerr, unsigned flags = 0, 
		    const char* pre = "") const;
  virtual void ddump() const;

  friend class ProcInsnIterator;

private:
  // Should not be used
  Proc() { } 
  Proc(const Proc& p) { }
  Proc& operator=(const Proc& p) { return *this; }

protected:
private:
  TextSeg*    sec; // we do not own
  std::string name;
  std::string linkname;
  Type        type;
  
  VMA begVMA; // points to the beginning of the first instruction
  VMA endVMA; // points to the beginning of the last instruction
  VMA size;

  // symbolic information: may or may not be known
  std::string filenm;  // filename and 
  suint       begLine; //   begin line of definition, if known
  Proc*       mParent; // parent routine, if lexically nested

  suint id;    // a unique identifier
  suint numInsns;
  
  static suint nextId;
};

} // namespace binutils


//***************************************************************************
// ProcInsnIterator
//***************************************************************************

namespace binutils {

// --------------------------------------------------------------------------
// 'ProcInsnIterator' enumerates a procedure's
// instructions maintaining relative order.
// --------------------------------------------------------------------------

class ProcInsnIterator {
public:
  ProcInsnIterator(const Proc& _p);
  ~ProcInsnIterator();

  // Returns the current object or NULL
  Insn* Current() const {
    if (it != endIt) { return (*it).second; }
    else { return NULL; }    
  }

  // Note: This is the 'operation VMA' and may not actually be the true
  // VMA!  Use the 'Insn' for the true VMA. 
  VMA CurrentVMA() const {
    if (it != endIt) { return (*it).first; }
    else { return 0; } 
  }
  
  void operator++()    { ++it; } // prefix increment
  void operator++(int) { it++; } // postfix increment

  bool IsValid() const { return (it != endIt); } 
  bool IsEmpty() const { return (it == endIt); }

  // Reset and prepare for iteration again
  void Reset();

private:
  // Should not be used
  ProcInsnIterator();
  ProcInsnIterator(const ProcInsnIterator& i);
  ProcInsnIterator& operator=(const ProcInsnIterator& i) { return *this; }

protected:
private:
  const Proc& p;
  const LM& lm;
  LM::VMAToInsnMap::const_iterator it;
  LM::VMAToInsnMap::const_iterator endIt;
};

} // namespace binutils


//***************************************************************************

#endif // binutils_Proc_hpp
