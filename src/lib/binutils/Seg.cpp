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

#include <cstring>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/gnu_bfd.h>

#include "LM.hpp"
#include "Seg.hpp"
#include "Proc.hpp"
#include "Insn.hpp"

#include "Dbg-LM.hpp"
#include "Dbg-Proc.hpp"

#include "BinUtils.hpp"

#include <lib/isa/ISA.hpp>

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// Seg
//***************************************************************************

BinUtil::Seg::Seg(BinUtil::LM* lm, const string& name, Type type,
		  VMA beg, VMA end, VMA size)
  : m_lm(lm), m_name(name), m_type(type), 
    m_begVMA(beg), m_endVMA(end), m_size(size)
{
}


BinUtil::Seg::~Seg()
{
  m_lm = NULL; 
}


string
BinUtil::Seg::toString(int flags, const char* pre) const
{
  std::ostringstream os;
  dump(os, flags, pre);
  return os.str();
}


void
BinUtil::Seg::dump(std::ostream& o, GCC_ATTR_UNUSED int flags,
		   const char* pre) const
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
BinUtil::Seg::ddump() const
{
  dump(std::cerr);
}

//***************************************************************************
// TextSeg
//***************************************************************************

BinUtil::TextSeg::TextSeg(BinUtil::LM* lm, const string& name, 
			  VMA beg, VMA end, uint64_t size)
  : Seg(lm, name, Seg::TypeText, beg, end, size), 
    m_contents(NULL), m_contentsRaw(NULL)
{
  uint rflg = m_lm->readFlags();
  if (rflg & LM::ReadFlg_fProc) {
    ctor_initProcs();
  }
  if (rflg & LM::ReadFlg_fInsn) {
    ctor_readSegment();
    ctor_disassembleProcs();
  }
}


BinUtil::TextSeg::~TextSeg()
{
  // Clear procedures
  for (ProcVec::iterator it = m_procs.begin(); it != m_procs.end(); ++it) {
    delete *it; // Proc*
  }

  // BFD info
  delete[] m_contentsRaw;
  m_contentsRaw = NULL;
  m_contents = NULL;
}


void
BinUtil::TextSeg::dump(std::ostream& o, int flags, const char* pre) const
{
  string pfx(pre);
  string pfx1 = pfx + "  ";

  Seg::dump(o, flags, pre);
  o << pfx << "  Procedures (" << numProcs() << ")\n";
  for (ProcVec::const_iterator it = m_procs.begin(); 
       it != m_procs.end(); ++it) {
    Proc* x = *it;
    x->dump(o, flags, pfx1.c_str());
  }
}


//***************************************************************************


void
BinUtil::TextSeg::ctor_initProcs()
{
  Dbg::LM* dbgInfo = m_lm->getDebugInfo();

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
  asymbol** symtab = m_lm->bfdSymTab(); // sorted
  uint symtabSz = m_lm->bfdSymTabSz();

  // FIXME:PERF: exploit sortedness of 'symtab' to start iteration
  for (uint i = 0; i < symtabSz; i++) {
    asymbol* sym = symtab[i];
    if (isIn(bfd_asymbol_value(sym)) && Proc::isProcBFDSym(sym)) {
      // NOTE: initially we have [begVMA, endVMA) where endVMA is the
      // *end* of the last insn.  This is changed after decoding below.
      VMA begVMA = bfd_asymbol_value(sym);
      VMA endVMA = 0; 
      
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
	DIAG_Assert(proc->begVMA() == begVMA, "TextSeg::ctor_initProcs: Procedure beginning at 0x" << hex << begVMA << " overlaps with:\n" << proc->toString());
	if (procType == Proc::Global) {
	  // 'global' types take precedence
	  proc->type(procType);
	}
	continue;
      }
      
      // Create a procedure based on best information we have.  We
      // always prefer explicit debug information over that inferred
      // from the symbol table.
      string procNm;
      string symNm = bfd_asymbol_name(sym);

      Dbg::LM::iterator it = dbgInfo->find(begVMA);
      Dbg::Proc* dbg = (it != dbgInfo->end()) ? it->second : NULL;

      if (!dbg) {
	procNm = findProcName(abfd, sym);
	string pnm = BinUtil::canonicalizeProcName(procNm);
	
	Dbg::LM::iterator1 it1 = dbgInfo->find1(pnm);
	dbg = (it1 != dbgInfo->end1()) ? it1->second : NULL;
      }
      if (!dbg) {
	Dbg::LM::iterator1 it1 = dbgInfo->find1(symNm);
	dbg = (it1 != dbgInfo->end1()) ? it1->second : NULL;
      }
      
      // Finding the end VMA (end of last insn).  The computation is
      // as follows because sometimes the debug information is
      // *wrong*. (Intel 9 has generated significant over-estimates).
      //
      // N.B. exploits the fact that the symbol table is sorted by vma
      VMA endVMA_approx = findProcEnd(i);

      if (dbg) {
	if (!dbg->name.empty()) {
	  procNm = dbg->name;
	}
	else if (!symNm.empty()) {
          // sometimes a procedure name is in the symbol table even
          // though it is not in the dwarf section. this case occurs
          // when gcc outlines routines from OpenMP parallel sections.
          procNm = symNm;
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
      uint size = endVMA - begVMA;

      if (size == 0) {
	continue;
      }
      
      // We now have a valid procedure.  Initilize with [begVMA, endVMA),
      // but note this is changed after disassembly.
      proc = new Proc(this, procNm, symNm, procType, begVMA, endVMA, size);
      m_procs.push_back(proc);
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
  if (numProcs() == 0) {
    // [begVMA, endVMA)
    Proc* proc = new Proc(this, name(), name(), Proc::Quasi, 
			  begVMA(), endVMA(), size());
    m_procs.push_back(proc);
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
    if (parent == child) {
      DIAG_WMsg(0, "Procedure has itself as parent!\n" << child->toString());
      continue; // skip
    }
    child->parent(parent);
  }
}


// Read in the section data (usually raw instructions).
void
BinUtil::TextSeg::ctor_readSegment()
{
  // - Obtain a new buffer, and align the pointer to a 16-byte
  //   boundary.
  // - We also add a 16 byte buffer at the beginning of the contents.
  //   This is because some of the GNU decoders (e.g. Sparc) want to
  //   examine both an instruction and its predecessor at the same
  //   time.  Since we do not want to tell them about text section
  //   sizes -- the ISA classes are independent of these details -- we
  //   add this padding to prevent array access errors when decoding
  //   the first instruction.
  
  // FIXME: Does "new" provide a way of returning an aligned pointer?
  m_contentsRaw = new char[size()+16+16];
  memset(m_contentsRaw, 0, 16+16);        // zero the padding
  char* contentsTmp = m_contentsRaw + 16; // add the padding
  m_contents = (char *)( ((uintptr_t)contentsTmp + 15) & ~15 ); // align

  bfd* abfd = m_lm->abfd();
  asection* bfdSeg = bfd_get_section_by_name(abfd, name().c_str());
  int ret = bfd_get_section_contents(abfd, bfdSeg, m_contents, 0, size());
  if (!ret) {
    delete[] m_contentsRaw;
    m_contentsRaw = m_contents = NULL;
    DIAG_EMsg("Error reading section: " << bfd_errmsg(bfd_get_error()));
    return;
  }
}


// Disassemble the instructions in each procedure
void
BinUtil::TextSeg::ctor_disassembleProcs()
{
  // ------------------------------------------------------------
  // Disassemble the instructions in each procedure.
  // ------------------------------------------------------------
  VMA sectionBase = begVMA();
  
  for (ProcVec::iterator it = m_procs.begin(); it != m_procs.end(); ++it) {
    Proc* p = *it;
    VMA procBeg = p->begVMA();
    VMA procEnd = p->endVMA();
    ushort insnSz = 0;
    VMA lastInsnVMA = procBeg; // vma of last valid instruction in the proc

    // Iterate over each vma at which an instruction might begin
    for (VMA vma = procBeg; vma < procEnd; ) {
      MachInsn *mi = &(m_contents[vma - sectionBase]);
      insnSz = LM::isa->getInsnSize(mi);
      if (insnSz == 0) {
	// This is not a recognized instruction (cf. data on CISC ISAs).
	++vma; // Increment the VMA, and try to decode again.
	continue;
      }

      int num_ops = LM::isa->getInsnNumOps(mi);
      if (num_ops == 0) {
	// This instruction contains data.  No need to decode.
	vma += insnSz;
	continue;
      }

      // We have a valid instruction at this vma!
      lastInsnVMA = vma;
      for (ushort opIndex = 0; opIndex < num_ops; opIndex++) {
        Insn *newInsn = makeInsn(m_lm->abfd(), mi, vma, opIndex, insnSz);
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
BinUtil::TextSeg::findProcName(bfd* abfd, asymbol* procSym) const
{
  string procName;

  // cf. LM::findSrcCodeInfo()
  asection* bfdSeg = bfd_get_section_by_name(abfd, name().c_str());

  bfd_boolean bfd_fnd = false;
  const char* bfd_func = NULL;

  if (bfdSeg) {
    bfd_vma secBase = bfd_section_vma(abfd, bfdSeg);
    bfd_vma symVal = bfd_asymbol_value(procSym);
    
    const char* file = NULL;
    uint line = 0;
    bfd_fnd = bfd_find_nearest_line(abfd, bfdSeg, m_lm->bfdSymTab(),
				    symVal - secBase, &file, &bfd_func, &line);
  }

  if (bfd_fnd && bfd_func && bfd_func[0] != '\0') {
    procName = bfd_func;
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
BinUtil::TextSeg::findProcEnd(int funcSymIndex) const
{
  // Since the symbol table we get is sorted by VMA, we can stop
  // the search as soon as we've gone beyond the VMA of this section.
  asymbol** symtab = m_lm->bfdSymTab();
  uint symtabSz = m_lm->bfdSymTabSz();

  VMA ret = endVMA();
  for (uint next = funcSymIndex + 1; next < symtabSz; ++next) {
    asymbol* sym = symtab[next];
    if (!isIn(bfd_asymbol_value(sym))) {
      break;
    }
    if (Proc::isProcBFDSym(sym)) {
      ret = bfd_asymbol_value(sym); 
      break;
    } 
  }
  return ret;
}


// Returns a new instruction of the appropriate type.  Promises not to
// return NULL.
BinUtil::Insn*
BinUtil::TextSeg::makeInsn(bfd* abfd, MachInsn* mi, VMA vma, ushort opIndex,
			   ushort sz) const
{
  // Assume that there is only one instruction type per
  // architecture (unlike i860 for example).
  Insn *newInsn = NULL;
  switch (bfd_get_arch(abfd)) {
    case bfd_arch_mips:
    case bfd_arch_alpha:
    case bfd_arch_powerpc:
    case bfd_arch_sparc:
      newInsn = new RISCInsn(mi, vma);
      break;
    case bfd_arch_i386:
#ifdef bfd_mach_k1om
    case bfd_arch_k1om:
#endif
      newInsn = new CISCInsn(mi, vma, sz);
      break;
    case bfd_arch_ia64:
      newInsn = new VLIWInsn(mi, vma, opIndex);
      break;
    default:
      DIAG_Die("TextSeg::makeInsn encountered unknown instruction type!");
  }
  return newInsn;
}


//***************************************************************************
