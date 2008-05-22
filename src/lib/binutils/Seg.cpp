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

//************************* System Include Files ****************************

#include <iostream>
using std::cerr;
using std::endl;
using std::hex;
using std::dec;

#include <sstream>

#include <string>
using std::string;

#include <map>

#include <algorithm>

//*************************** User Include Files ****************************

#include <include/gnu_bfd.h>

#include "LM.hpp"
#include "Seg.hpp"
#include "Proc.hpp"
#include "Insn.hpp"

#include "dbg_LM.hpp"
#include "dbg_Proc.hpp"

#include "BinUtils.hpp"

#include <lib/isa/ISA.hpp>

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// Seg
//***************************************************************************

binutils::Seg::Seg(binutils::LM* lm, const string& name, Type type,
		   VMA beg, VMA end, VMA size)
  : m_lm(lm), m_name(name), m_type(type), 
    m_begVMA(beg), m_endVMA(end), m_size(size)
{
}


binutils::Seg::~Seg()
{
  m_lm = NULL; 
}


string
binutils::Seg::toString(int flags, const char* pre) const
{
  std::ostringstream os;
  dump(os, flags, pre);
  return os.str();
}


void
binutils::Seg::dump(std::ostream& o, int flags, const char* pre) const
{
  string p(pre);
  o << std::showbase;
  o << p << "------------------- Section Dump ------------------\n";
  o << p << "  Name: `" << name() << "'\n";
  o << p << "  Type: `";
  switch (type()) {
    case TypeBSS:  o << "BSS'\n";  break;
    case TypeText: o << "Text'\n"; break;
    case TypeData: o << "Data'\n"; break;
    default:   DIAG_Die("Unknown segment type");
  }
  o << p << "  VMA: [" << hex << begVMA() << ", " << endVMA() << dec << ")\n";
  o << p << "  Size(b): " << size() << "\n";
}


void
binutils::Seg::ddump() const
{
  dump(std::cerr);
}

//***************************************************************************
// TextSeg
//***************************************************************************

binutils::TextSeg::TextSeg(binutils::LM* lm, const string& name, 
			   VMA beg, VMA end, uint64_t size)
  : Seg(lm, name, Seg::TypeText, beg, end, size), 
    m_procedures(0),
    m_contentsRaw(NULL), m_contents(NULL)
{
  // 1. Initialize procedures
  Create_InitializeProcs();

  // ------------------------------------------------------------
  // 2. Read in the section data (usually raw instructions).
  // ------------------------------------------------------------
  
  // Obtain a new buffer, and align the pointer to a 16-byte boundary.
  // We also add a 16 byte buffer at the beginning of the contents.
  //   This is because some of the GNU decoders (e.g. Sparc) want to
  //   examine both an instruction and its predecessor at the same time.
  //   Since we do not want to tell them about text section sizes -- the
  //   ISA classes are independent of these details -- we add this
  //   padding to prevent array access errors when decoding the first
  //   instruciton.
  
  // FIXME: Does "new" provide a way of returning an aligned pointer?
  m_contentsRaw = new char[size+16+16];
  memset(m_contentsRaw, 0, 16+16);        // zero the padding
  char* contentsTmp = m_contentsRaw + 16; // add the padding
  m_contents = (char *)( ((uintptr_t)contentsTmp + 15) & ~15 ); // align

  const char *nameStr = name.c_str();
  bfd* abfd = m_lm->abfd();
  int result = bfd_get_section_contents(abfd,
                                        bfd_get_section_by_name(abfd, nameStr),
                                        m_contents, 0, size);
  if (!result) {
    delete[] m_contentsRaw;
    m_contentsRaw = m_contents = NULL;
    DIAG_EMsg("Error reading section: " << bfd_errmsg(bfd_get_error()));
    return;
  }
  
  // 3. Disassemble the instructions in each procedure
  Create_DisassembleProcs();
}


binutils::TextSeg::~TextSeg()
{
  delete[] m_contentsRaw;
  m_contentsRaw = NULL;
  m_contents = NULL;

  // Clear procedures
  for (TextSegProcIterator it(*this); it.IsValid(); ++it) {
    delete it.Current(); // Proc*
  }
  m_procedures.clear();
}


void
binutils::TextSeg::dump(std::ostream& o, int flags, const char* pre) const
{
  string p(pre);
  string p1 = p + "  ";

  Seg::dump(o, flags, pre);
  o << p << "  Procedures (" << GetNumProcs() << ")\n";
  for (TextSegProcIterator it(*this); it.IsValid(); ++it) {
    Proc* p = it.Current();
    p->dump(o, flags, p1.c_str());
  }
}


//***************************************************************************


void
binutils::TextSeg::Create_InitializeProcs()
{
  dbg::LM* dbgInfo = m_lm->GetDebugInfo();

  // Any procedure with a parent has a <Proc*, parentVMA> entry
  std::map<Proc*, VMA> parentMap;
  
  // ------------------------------------------------------------
  // Each text section finds and creates its own routines.
  // Traverse the symbol table (which is sorted by VMA) searching
  // for function symbols in our section.  Create a Proc for
  // each one found.
  //
  // Note that symbols can appear multiple times (e.g. a weak symbol
  // 'sbrk' along with a gloabl symbol '__sbrk'), but we should not
  // have multiple procedures.
  // ------------------------------------------------------------

  bfd* abfd = m_lm->abfd();
  asymbol** symtab = m_lm->bfdSymTab();
  uint symtabSz = m_lm->bfdSymTabSz();

  for (int i = 0; i < symtabSz; i++) {
    // FIXME: exploit the fact that the symbol table is sorted by vma
    asymbol* sym = symtab[i]; 
    if (isIn(bfd_asymbol_value(sym)) 
	&& (sym->flags & BSF_FUNCTION)
        && !bfd_is_und_section(sym->section)) {
      
      VMA begVMA = bfd_asymbol_value(sym);
      VMA endVMA = 0; // see note above
      
      Proc::Type procType;
      if (sym->flags & BSF_LOCAL) {
        procType = Proc::Local;
      }
      else if (sym->flags & BSF_WEAK) {
        procType = Proc::Weak;
      }
      else if (sym->flags & BSF_GLOBAL) {
        procType = Proc::Global;
      }
      else {
        procType = Proc::Unknown;
      }
      
      Proc* proc = m_lm->findProc(begVMA);
      if (proc) {
	DIAG_Assert(proc->begVMA() == begVMA, "TextSeg::Create_InitializeProcs: Procedure beginning at 0x" << hex << begVMA << " overlaps with:\n" << proc->toString());
	if (procType == Proc::Global) {
	  // 'global' types take precedence
	  proc->type(procType);
	}
	continue;
      }
      
      // Create a procedure based on best information we have.  We
      // always prefer explicit debug information over that inferred
      // from the symbol table.
      // NOTE: Initially, the end addr is the *end* of the last insn.
      // This is changed after decoding below.
      string procNm;
      string symNm = bfd_asymbol_name(sym);

      dbg::Proc* dbg = (*dbgInfo)[begVMA];
      if (!dbg) {
	procNm = FindProcName(abfd, sym);
	string pnm = GetBestFuncName(procNm);
	dbg = (*dbgInfo)[pnm];
      }
      if (!dbg) {
	dbg = (*dbgInfo)[symNm];
      }
      
      // Finding the end VMA (end of last insn).  The computation is
      // as follows because sometimes the debug information is
      // *wrong*. (Intel 9 has generated significant over-estimates).
      VMA endVMA_approx = FindProcEnd(i);
      if (dbg) {
	if (!dbg->name.empty()) {
	  procNm = dbg->name;
	}

#if 1
	// Remove capability below... the DWARF sizes can be wrong!!
	endVMA = endVMA_approx;
#else
	endVMA = std::min(dbg->endVMA, endVMA_approx);
	if (endVMA != endVMA_approx) {
	  int64_t diff = endVMA - endVMA_approx;
	  DIAG_DevMsg(0, procNm << ": inconsistent end VMA: " << diff << " [" << std::showbase << std::hex << begVMA << "-" << endVMA << "/" << endVMA_approx << std::dec << "]");
	}
#endif
      }
      if (!dbg || endVMA == 0) {
	endVMA = endVMA_approx;
      }
      uint size = endVMA - begVMA; // see note above

      if (size == 0) {
	continue;
      }
      
      // We now have a valid procedure
      proc = new Proc(this, procNm, symNm, procType, begVMA, endVMA, size);
      m_procedures.push_back(proc);
      m_lm->insertProc(VMAInterval(begVMA, endVMA), proc);

      // Add symbolic info
      if (dbg) {
	proc->filename(dbg->filenm);
	proc->begLine(dbg->begLine);
	if (dbg->parent) {
	  parentMap.insert(std::make_pair(proc, dbg->parent->begVMA));
	}
      }
    }
  }

  // ------------------------------------------------------------
  //  If a text section does not have any function symbols, consider
  //  the whole section a quasi procedure
  // ------------------------------------------------------------
  if (GetNumProcs() == 0) {
    Proc* proc = new Proc(this, name(), name(), Proc::Quasi, 
			  begVMA(), endVMA(), size());
    m_procedures.push_back(proc);
    m_lm->insertProc(VMAInterval(begVMA(), endVMA()), proc);
  }


  // ------------------------------------------------------------
  // Embed parent information
  // ------------------------------------------------------------
  for (std::map<Proc*, VMA>::iterator it = parentMap.begin(); 
       it != parentMap.end(); ++it) {
    Proc* child = it->first;
    VMA parentVMA = it->second;
    Proc* parent = m_lm->findProc(parentVMA);
    DIAG_AssertWarn(parent, "Could not find parent within this section:\n" 
		    << child->toString());
    DIAG_Assert(parent != child, "Procedure has itself as parent!\n" 
		<< child->toString());
    child->parent(parent);
  }
}


void
binutils::TextSeg::Create_DisassembleProcs()
{
  // ------------------------------------------------------------
  // Disassemble the instructions in each procedure.
  // ------------------------------------------------------------
  VMA sectionBase = begVMA();
  
  for (TextSegProcIterator it(*this); it.IsValid(); ++it) {
    Proc* p = it.Current();
    VMA procBeg = p->begVMA();
    VMA procEnd = p->endVMA();
    ushort insnSz = 0;
    VMA lastInsnVMA = procBeg; // vma of last valid instruction in the proc

    // Iterate over each vma at which an instruction might begin
    for (VMA vma = procBeg; vma < procEnd; ) {
      MachInsn *mi = &(m_contents[vma - sectionBase]);
      insnSz = LM::isa->GetInsnSize(mi);
      if (insnSz == 0) {
	// This is not a recognized instruction (cf. data on CISC ISAs).
	++vma; // Increment the VMA, and try to decode again.
	continue;
      }

      int num_ops = LM::isa->GetInsnNumOps(mi);
      if (num_ops == 0) {
	// This instruction contains data.  No need to decode.
	vma += insnSz;
	continue;
      }

      // We have a valid instruction at this vma!
      lastInsnVMA = vma;
      for (ushort opIndex = 0; opIndex < num_ops; opIndex++) {
        Insn *newInsn = MakeInsn(m_lm->abfd(), mi, vma, opIndex, insnSz);
        m_lm->insertInsn(vma, opIndex, newInsn); 
      }
      vma += insnSz; 
    }
    // 'insnSz' is now the size of the last instruction or 0

    // Now we can update the procedure's end address and size since we
    // know where the last instruction begins.  The procedure's
    // original end address was guessed to be the begin address of the
    // following procedure while determining all procedures above.
    p->endVMA(lastInsnVMA);
    p->size(p->endVMA() - p->begVMA() + insnSz); 
  }
}


// Returns the name of the procedure referenced by 'procSym' using
// debugging information, if possible; otherwise returns the symbol
// name.
string
binutils::TextSeg::FindProcName(bfd* abfd, asymbol* procSym) const
{
  string procName;
  const char* func = NULL, * file = NULL;
  uint bfd_line = 0;

  // cf. LM::GetSourceFileInfo
  asection* bfdSeg = bfd_get_section_by_name(abfd, name().c_str());
  bfd_vma secBase = bfd_section_vma(abfd, bfdSeg);
  bfd_vma symVal = bfd_asymbol_value(procSym);
  if (bfdSeg) {
    bfd_find_nearest_line(abfd, bfdSeg, m_lm->bfdSymTab(),
			  symVal - secBase, &file, &func, &bfd_line);
  }

  if (func && (strlen(func) > 0)) {
    procName = func;
  } 
  else {
    procName = bfd_asymbol_name(procSym); 
  }
  
  return procName;
}


// Approximate the end VMA of the function given by funcSymIndex.
// This is normally the address of the next function symbol in this
// section.  However, if this is the last function in the section,
// then it is the address of the end of the section.  One can safely
// assume this returns an over-estimate of the end VMA.
VMA
binutils::TextSeg::FindProcEnd(int funcSymIndex) const
{
  // Since the symbol table we get is sorted by VMA, we can stop
  // the search as soon as we've gone beyond the VMA of this section.
  asymbol** symtab = m_lm->bfdSymTab();
  uint symtabSz = m_lm->bfdSymTabSz();

  VMA ret = endVMA();
  for (int next = funcSymIndex + 1; next < symtabSz; ++next) {
    asymbol* sym = symtab[next];
    if (!isIn(bfd_asymbol_value(sym))) {
      break;
    }
    if ((sym->flags & BSF_FUNCTION) && !bfd_is_und_section(sym->section)) {
      ret = bfd_asymbol_value(sym); 
      break;
    } 
  }
  return ret;
}


// Returns a new instruction of the appropriate type.  Promises not to
// return NULL.
binutils::Insn*
binutils::TextSeg::MakeInsn(bfd* abfd, MachInsn* mi, VMA vma, ushort opIndex,
			     ushort sz) const
{
  // Assume that there is only one instruction type per
  // architecture (unlike i860 for example).
  Insn *newInsn = NULL;
  switch (bfd_get_arch(abfd)) {
    case bfd_arch_mips:
    case bfd_arch_alpha:
    case bfd_arch_sparc:
      newInsn = new RISCInsn(mi, vma);
      break;
    case bfd_arch_i386:
      newInsn = new CISCInsn(mi, vma, sz);
      break;
    case bfd_arch_ia64:
      newInsn = new VLIWInsn(mi, vma, opIndex);
      break;
    default:
      DIAG_Die("TextSeg::MakeInsn encountered unknown instruction type!");
  }
  return newInsn;
}


//***************************************************************************
// TextSegProcIterator
//***************************************************************************

binutils::TextSegProcIterator::TextSegProcIterator(const TextSeg& _sec)
  : sec(_sec)
{
  Reset();
}

binutils::TextSegProcIterator::~TextSegProcIterator()
{
}

//***************************************************************************
