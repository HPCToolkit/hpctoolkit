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

binutils::Seg::Seg(binutils::LM* _lm, string& _name, Type t,
		   VMA _beg, VMA _end, VMA _sz)
  : lm(_lm), name(_name), type(t), beg(_beg), end(_end), size(_sz)
{
}


binutils::Seg::~Seg()
{
  lm = NULL; 
}


string
binutils::Seg::toString(int flags, const char* pre) const
{
  std::ostringstream os;
  dump(os, flags, pre);
  os << std::ends;
  return os.str();
}


void
binutils::Seg::dump(std::ostream& o, int flags, const char* pre) const
{
  string p(pre);
  o << p << "------------------- Section Dump ------------------\n";
  o << p << "  Name: `" << GetName() << "'\n";
  o << p << "  Type: `";
  switch (GetType()) {
    case BSS:  o << "BSS'\n";  break;
    case Text: o << "Text'\n"; break;
    case Data: o << "Data'\n"; break;
    default:   DIAG_Die("Unknown segment type");
  }
  o << p << "  VMA: [0x" << hex << GetBeg() << ", 0x"
    << GetEnd() << dec << ")\n";
  o << p << "  Size(b): " << GetSize() << "\n";
}


void
binutils::Seg::ddump() const
{
  dump(std::cerr);
}

//***************************************************************************
// TextSeg
//***************************************************************************

class binutils::TextSegImpl { 
public:
  TextSegImpl() 
    : abfd(NULL), symTable(NULL), 
      contentsRaw(NULL), contents(NULL), numSyms(0) { }
  ~TextSegImpl() { }
    
  bfd* abfd;          // we do not own
  asymbol **symTable; // we do not own
  char *contentsRaw; // allocated memory for section contents (we own)
  char *contents;    // contents, aligned with a 16-byte boundary
  int numSyms;
};


binutils::TextSeg::TextSeg(binutils::LM* _lm, string& _name, 
			   VMA _beg, VMA _end, uint64_t _size, 
			   asymbol** syms, int numSyms, bfd* abfd)
  : Seg(_lm, _name, Seg::Text, _beg, _end, _size), impl(NULL),
    procedures(0)
{
  impl = new TextSegImpl;
  impl->abfd = abfd;     // we do not own 'abfd'
  impl->symTable = syms; // we do not own 'syms'
  impl->numSyms = numSyms;
  impl->contents = NULL;

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
  impl->contentsRaw = new char[_size+16+16];
  memset(impl->contentsRaw, 0, 16+16);        // zero the padding
  char* contentsTmp = impl->contentsRaw + 16; // add the padding
  impl->contents = (char *)( ((uintptr_t)contentsTmp + 15) & ~15 ); // align

  const char *nameStr = _name.c_str();
  int result = bfd_get_section_contents(abfd,
                                        bfd_get_section_by_name(abfd, nameStr),
                                        impl->contents, 0, _size);
  if (!result) {
    delete [] impl->contentsRaw;
    impl->contentsRaw = impl->contents = NULL;
    cerr << "Error reading section contents: " << bfd_errmsg(bfd_get_error())
	 << endl;
    return;
  }
  
  // 3. Disassemble the instructions in each procedure
  Create_DisassembleProcs();
}


binutils::TextSeg::~TextSeg()
{
  // Clear impl
  impl->symTable = NULL;
  delete[] impl->contentsRaw; impl->contentsRaw = NULL;
  impl->contents = NULL;
  delete impl;
    
  // Clear procedures
  for (TextSegProcIterator it(*this); it.IsValid(); ++it) {
    delete it.Current(); // Proc*
  }
  procedures.clear();
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
  LM* lm = GetLM();
  dbg::LM* dbgInfo = lm->GetDebugInfo();

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
  for (int i = 0; i < impl->numSyms; i++) {
    // FIXME: exploit the fact that the symbol table is sorted by vma
    asymbol* sym = impl->symTable[i]; 
    if (IsIn(bfd_asymbol_value(sym)) && (sym->flags & BSF_FUNCTION)
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
      
      Proc* proc = lm->findProc(begVMA);
      if (proc) {
	DIAG_Assert(proc->GetBegVMA() == begVMA, "TextSeg::Create_InitializeProcs: Procedure beginning at 0x" << hex << begVMA << " overlaps with:\n" << proc->toString());
	if (procType == Proc::Global) {
	  // 'global' types take precedence
	  proc->type() = procType;
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
	procNm = FindProcName(impl->abfd, sym);
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
	endVMA = MIN(dbg->endVMA, endVMA_approx);
	if (!dbg->name.empty()) {
	  procNm = dbg->name;
	}
      }
      if (!dbg || endVMA == 0) {
	endVMA = endVMA_approx;
      }
      unsigned int size = endVMA - begVMA; // see note above

      if (size == 0) {
	continue;
      }
      
      // We now have a valid procedure
      proc = new Proc(this, procNm, symNm, procType, begVMA, endVMA, size);
      procedures.push_back(proc);
      lm->insertProc(VMAInterval(begVMA, endVMA), proc);

      // Add symbolic info
      if (dbg) {
	proc->GetFilename() = dbg->filenm;
	proc->GetBegLine() = dbg->begLine;
	if (dbg->parent) {
	  parentMap.insert(std::make_pair(proc, dbg->parent->begVMA));
	}
      }
    }
  }

  // ------------------------------------------------------------
  // Embed parent information
  // ------------------------------------------------------------
  for (std::map<Proc*, VMA>::iterator it = parentMap.begin(); 
       it != parentMap.end(); ++it) {
    Proc* child = it->first;
    VMA parentVMA = it->second;
    Proc* parent = lm->findProc(parentVMA);
    DIAG_AssertWarn(parent, "Could not find parent within this section:\n" 
		    << child->toString());
    DIAG_Assert(parent != child, "Procedure has itself as parent!\n" 
		<< child->toString());
    child->parent() = parent;
  }
}


void
binutils::TextSeg::Create_DisassembleProcs()
{
  LM* lm = GetLM();
  bfd* abfd = impl->abfd;
  
  // ------------------------------------------------------------
  // Disassemble the instructions in each procedure.
  // ------------------------------------------------------------
  VMA sectionBase = GetBeg();
  
  for (TextSegProcIterator it(*this); it.IsValid(); ++it) {
    Proc* p = it.Current();
    VMA procBeg = p->GetBegVMA();
    VMA procEnd = p->GetEndVMA();
    ushort insnSz = 0;
    VMA lastInsnVMA = procBeg; // vma of last valid instruction in the proc

    // Iterate over each vma at which an instruction might begin
    for (VMA vma = procBeg; vma < procEnd; ) {
      MachInsn *mi = &(impl->contents[vma - sectionBase]);
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
        Insn *newInsn = MakeInsn(abfd, mi, vma, opIndex, insnSz);
        lm->insertInsn(vma, opIndex, newInsn); 
      }
      vma += insnSz; 
    }
    // 'insnSz' is now the size of the last instruction or 0

    // Now we can update the procedure's end address and size since we
    // know where the last instruction begins.  The procedure's
    // original end address was guessed to be the begin address of the
    // following procedure while determining all procedures above.
    p->SetEndVMA(lastInsnVMA);
    p->SetSize(p->GetEndVMA() - p->GetBegVMA() + insnSz); 
  }
}


// Returns the name of the procedure referenced by 'procSym' using
// debugging information, if possible; otherwise returns the symbol
// name.
string
binutils::TextSeg::FindProcName(bfd *abfd, asymbol *procSym) const
{
  string procName;
  const char* func = NULL, * file = NULL;
  unsigned int bfd_line = 0;

  // cf. LM::GetSourceFileInfo
  asection *bfdSeg = bfd_get_section_by_name(abfd, GetName().c_str());
  bfd_vma secBase = bfd_section_vma(abfd, bfdSeg);
  bfd_vma symVal = bfd_asymbol_value(procSym);
  if (bfdSeg) {
    bfd_find_nearest_line(abfd, bfdSeg, impl->symTable,
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
  VMA ret = GetEnd();
  for (int next = funcSymIndex + 1; next < impl->numSyms; next++) {
    asymbol *sym = impl->symTable[next];
    if (!IsIn(bfd_asymbol_value(sym))) {
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
binutils::TextSeg::MakeInsn(bfd *abfd, MachInsn* mi, VMA vma, ushort opIndex,
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
