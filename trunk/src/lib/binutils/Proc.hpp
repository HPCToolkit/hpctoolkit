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
  enum Type { Local, Weak, Global, Quasi, Unknown };
  
public:
  // -------------------------------------------------------  
  // Constructor/Destructor
  // -------------------------------------------------------
  Proc(TextSeg* seg, const std::string& name, const std::string& linkname,
       Type t, VMA begVMA, VMA endVMA, unsigned int size);
  virtual ~Proc();


  // -------------------------------------------------------
  // Basic data
  // -------------------------------------------------------

  TextSeg* GetTextSeg() const { return m_seg; }
  LM*  GetLM()  const { return m_seg->GetLM(); }

  // Returns the name as determined by debugging information; if this
  // is unavailable returns the name found in the symbol table.  (Note
  // that no demangling is performed.)
  const std::string& name()  const { return m_name; }
  void               name(const std::string& name) { m_name = name; }

  // Returns the name as found in the symbol table
  const std::string& GetLinkName() const { return m_linkname; }

  // Return type of procedure
  Type type() const    { return m_type; }
  void type(Type type) { m_type = type; }

  // Return the begin and end virtual memory address of a procedure:
  // [beg, end].  Note that the end address points to the beginning of
  // the last instruction which is different than the convention used
  // for 'Seg'.
  VMA  begVMA() const     { return m_begVMA; }
  VMA  endVMA() const     { return m_endVMA; }
  void endVMA(VMA endVMA) { m_endVMA = endVMA; }

  // Return size, which is (endVMA - startVMA) + sizeof(last instruction)
  unsigned int size() const             { return m_size; }
  void         size(unsigned int size)  { m_size = size; }
  
  // -------------------------------------------------------
  // Symbolic information: availability depends upon debugging information
  // -------------------------------------------------------

  bool hasSymbolic() { return SrcFile::isValid(m_begLine); }

  const std::string& filename() const         { return m_filenm; }
  void               filename(std::string& x) { m_filenm = x; }

  SrcFile::ln  begLine() const        { return m_begLine; }
  void         begLine(SrcFile::ln x) { m_begLine = x; }

  Proc* parent() const  { return m_parent; }
  void  parent(Proc* x) { m_parent = x; }

  // -------------------------------------------------------
  //
  // -------------------------------------------------------

  // Return true if virtual memory address 'vma' is within the procedure
  // WARNING: vma must be unrelocated
  bool  isIn(VMA vma)  const { return (m_begVMA <= vma && vma <= m_endVMA); }

  // Return the unique number assigned to this procedure
  unsigned int id() const { return m_id; }

  // Return the number of instructions in the procedure (FIXME: never computed)
  unsigned int numInsns()  const { return m_numInsns; }

  // Return the first and last instruction in the procedure
  Insn* firstInsn() const { return findInsn(m_begVMA, 0); }
  Insn* lastInsn() const;
  
  // -------------------------------------------------------
  // Convenient wrappers for the 'LM' versions of the same.
  // -------------------------------------------------------

  MachInsn* findMachInsn(VMA vma, ushort &sz) const {
    return m_seg->GetLM()->findMachInsn(vma, sz);
  }
  Insn* findInsn(VMA vma, ushort opIndex) const {
    return m_seg->GetLM()->findInsn(vma, opIndex);
  }
  bool GetSourceFileInfo(VMA vma, ushort opIndex,
			 std::string& func, std::string& file, 
			 SrcFile::ln& line) const {
    return m_seg->GetLM()->GetSourceFileInfo(vma, opIndex, func, file, line);
  }
  bool GetSourceFileInfo(VMA begVMA, ushort bOpIndex,
			 VMA endVMA, ushort eOpIndex,
			 std::string& func, std::string& file,
			 SrcFile::ln& begLine, SrcFile::ln& endLine,
			 unsigned flags = 1) const {
    return m_seg->GetLM()->GetSourceFileInfo(begVMA, bOpIndex,
					     endVMA, eOpIndex, 
					     func, file, 
					     begLine, endLine, flags);
  }
  
  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  // Current meaning of 'flags'
  //   0 : short dump (without instructions)
  //   1 : full dump
  
  std::string toString(int flags = LM::DUMP_Short) const;

  virtual void dump(std::ostream& o = std::cerr, 
		    int flags = LM::DUMP_Short, const char* pre = "") const;
  void ddump() const;

  friend class ProcInsnIterator;

private:
  // Should not be used
  Proc() { } 
  Proc(const Proc& p) { }
  Proc& operator=(const Proc& p) { return *this; }

protected:
private:
  TextSeg*    m_seg; // we do not own
  std::string m_name;
  std::string m_linkname;
  Type        m_type;
  
  VMA m_begVMA; // points to the beginning of the first instruction
  VMA m_endVMA; // points to the beginning of the last instruction
  unsigned int m_size;

  // symbolic information: may or may not be known
  std::string m_filenm;  // filename and 
  SrcFile::ln m_begLine; //   begin line of definition, if known
  Proc*       m_parent; // parent routine, if lexically nested

  unsigned int m_id;    // a unique identifier
  unsigned int m_numInsns;
  
  static unsigned int nextId;
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
