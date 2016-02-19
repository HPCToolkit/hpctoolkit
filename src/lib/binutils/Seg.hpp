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

#ifndef BinUtil_Seg_hpp
#define BinUtil_Seg_hpp

//************************* System Include Files ****************************

#include <string>
#include <vector>
#include <iostream>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>
#include <include/gnu_bfd.h>

#include "LM.hpp"

#include <lib/isa/ISATypes.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************
// Seg
//***************************************************************************

namespace BinUtil {

class Proc;
class Insn;

// --------------------------------------------------------------------------
// 'Seg' is a base class for representing file segments/sections
// of a 'LoadModule'.
// --------------------------------------------------------------------------

class Seg {
public:
  enum Type { TypeNULL = 0, TypeBSS, TypeText, TypeData };
  
  Seg(LM* lm, const std::string& name, Type type,
      VMA beg, VMA end, uint64_t size);

  virtual ~Seg();

  LM*
  lm() const
  { return m_lm; }

  // Return name and type of section
  const std::string&
  name() const
  { return m_name; }
  
  Type
  type() const
  { return m_type; }
  
  // Return begin/end virtual memory address for section: [begVMA, endVMA).
  // Note that the end of a section is equal to the begin address of
  // the next section (or the end of the file) which is different than
  // the convention used for a 'Proc'.
  VMA
  begVMA() const
  { return m_begVMA; }

  VMA
  endVMA() const
  { return m_endVMA; }

  // Return size of section
  uint64_t
  size() const
  { return m_size; }

  // Return true if virtual memory address 'vma' is within the section
  // WARNING: vma must be unrelocated
  bool
  isIn(VMA vma) const
  { return (m_begVMA <= vma && vma < m_endVMA); }

  // Convenient wrappers for the 'LM' versions of the same.
  MachInsn*
  findMachInsn(VMA vma, ushort &sz) const
  { return m_lm->findMachInsn(vma, sz); }
  
  Insn*
  findInsn(VMA vma, ushort opIndex) const
  { return m_lm->findInsn(vma, opIndex); }

  bool
  findSrcCodeInfo(VMA vma, ushort opIndex,
		  std::string& func, std::string& file,
		  SrcFile::ln& line) const
  { return m_lm->findSrcCodeInfo(vma, opIndex, func, file, line); }

  bool
  findSrcCodeInfo(VMA begVMA, ushort bOpIndex,
		    VMA endVMA, ushort eOpIndex,
		    std::string& func, std::string& file,
		    SrcFile::ln& begLine, SrcFile::ln& endLine,
		    uint flags = 1) const
  {
    return m_lm->findSrcCodeInfo(begVMA, bOpIndex, endVMA, eOpIndex,
				 func, file, begLine, endLine, flags);
  }

  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  // Current meaning of 'flags'
  //   0 : short dump (without instructions)
  //   1 : full dump

  std::string
  toString(int flags = LM::DUMP_Short, const char* pre = "") const;

  virtual void
  dump(std::ostream& o = std::cerr,
       int flags = LM::DUMP_Short, const char* pre = "") const;
  
  void
  ddump() const;
  
protected:
  // Should not be used
  Seg()
  { }

  Seg(const Seg& GCC_ATTR_UNUSED s)
  { }

  Seg&
  operator=(const Seg& GCC_ATTR_UNUSED s)
  { return *this; }
  
protected:
  LM* m_lm; // do not own

private:
  std::string m_name;
  Type m_type;
  VMA  m_begVMA; // beginning of section
  VMA  m_endVMA; // end of section [equal to the beginning of next section]
  uint64_t m_size; // size in bytes
};

} // namespace BinUtil


//***************************************************************************
// TextSeg
//***************************************************************************

namespace BinUtil {


// --------------------------------------------------------------------------
// 'TextSeg' represents a text segment in a 'LM' and
// implements special functionality pertaining to it.
// --------------------------------------------------------------------------

class TextSeg : public Seg {
public:
  typedef std::vector<Proc*> ProcVec;

public:
  TextSeg(LM* lm, const std::string& name,
	  VMA beg, VMA end, uint64_t size);
  virtual ~TextSeg();

  // -------------------------------------------------------
  // Procedures
  // -------------------------------------------------------
  
  uint
  numProcs() const
  { return m_procs.size(); }
  
#if 0
  LM::ProcMap::iterator
  begin() {
    if (size() > 0) {
      return m_lm->procs().find(VMAInterval(begVMA(), begVMA() + 1));
    }
    else {
      return m_lm->procs().end();
    }
  }

  LM::ProcMap::const_iterator
  begin() const {
    return const_cast<TextSeg*>(this)->begin();
  }
  
  LM::ProcMap::iterator
  end() {
    if (size() > 0) {
      return m_lm->procs().find(VMAInterval(endVMA() - 1, endVMA()));
    }
    else {
      return m_lm->procs().end();
    }
  }

  LM::ProcMap::const_iterator
  end() const {
    return const_cast<TextSeg*>(this)->end();
  }
#endif


  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  virtual void
  dump(std::ostream& o = std::cerr,
       int flags = LM::DUMP_Short, const char* pre = "") const;
  
private:
  // Should not be used
  TextSeg()
  { }
  
  TextSeg(const TextSeg& GCC_ATTR_UNUSED s)
  { }

  TextSeg&
  operator=(const TextSeg& GCC_ATTR_UNUSED s)
  { return *this; }

  void
  ctor_initProcs();

  void
  ctor_readSegment();

  void
  ctor_disassembleProcs();

  // Construction helpers
  std::string
  findProcName(bfd* abfd, asymbol* procSym) const;

  VMA
  findProcEnd(int funcSymIndex) const;

  Insn*
  makeInsn(bfd* abfd, MachInsn* mi, VMA vma,
	   ushort opIndex, ushort sz) const;

protected:
private:
  ProcVec m_procs;
  char* m_contents;    // contents, aligned with a 16-byte boundary
  char* m_contentsRaw; // allocated memory for section contents (we own)
};


} // namespace BinUtil

//****************************************************************************

#endif // BinUtil_Seg_hpp
