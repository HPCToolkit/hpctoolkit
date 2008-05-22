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

#include <typeinfo>

#include <string>
using std::string;

#include <sstream>

#include <iostream>
using std::cerr;
using std::endl;
using std::hex;
using std::dec;

//*************************** User Include Files ****************************

#include <include/general.h>

#include "LM.hpp"
#include "Seg.hpp"
#include "Insn.hpp"
#include "Proc.hpp"

#include "dbg_LM.hpp"

#include <lib/isa/MipsISA.hpp>
#include <lib/isa/AlphaISA.hpp>
#include <lib/isa/SparcISA.hpp>
#include <lib/isa/x86ISA.hpp>
#include <lib/isa/IA64ISA.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/QuickSort.hpp>

//*************************** Forward Declarations **************************

#define DBG_BLD_PROC_MAP 0

//***************************************************************************

//***************************************************************************
// LM
//***************************************************************************

// current ISA (see comments in header)
ISA* binutils::LM::isa = NULL;


binutils::LM::LM()
  : m_type(Unknown),
    m_txtBeg(0), m_txtEnd(0), m_begVMA(0),
    m_textBegReloc(0), m_unRelocDelta(0),
    m_bfd(NULL), m_bfdSymTbl(NULL), m_bfdSymTblSort(NULL), 
    m_numSyms(0)
{
}


binutils::LM::~LM()
{
  // Clear sections
  for (LMSegIterator it(*this); it.IsValid(); ++it) {
    delete it.Current(); // Seg*
  }
  m_sections.clear();

  // BFD info
  if (m_bfd) {
    bfd_close(m_bfd);
    m_bfd = NULL;
  }

  delete[] m_bfdSymTbl;
  m_bfdSymTbl = NULL;

  delete[] m_bfdSymTblSort;
  m_bfdSymTblSort = NULL; 
    
  // Clear vmaToInsMap
  VMAToInsnMap::iterator it;
  for (it = m_vmaToInsnMap.begin(); it != m_vmaToInsnMap.end(); ++it) {
    delete (*it).second; // Insn*
  }
  m_vmaToInsnMap.clear();

  // reset isa
  delete isa;
  isa = NULL;
}


void
binutils::LM::open(const char* moduleName)
{
  DIAG_Assert(Logic::implies(!m_name.empty(), m_name.c_str() == moduleName), "Cannot open a different file!");
  
  // -------------------------------------------------------
  // 1. Initialize bfd and open the object file.
  // -------------------------------------------------------

  // Determine file existence.
  bfd_init();
  m_bfd = bfd_openr(moduleName, "default");
  if (!m_bfd) {
    BINUTILS_Throw("'" << moduleName << "': " << bfd_errmsg(bfd_get_error()));
  }
  
  // bfd_object: The BFD may contain data, symbols, relocations and
  //   debug info.
  // bfd_archive: The BFD contains other BFDs and an optional index.
  // bfd_core: The BFD contains the result of an executable core dump.
  if (!bfd_check_format(m_bfd, bfd_object)) {
    BINUTILS_Throw("'" << moduleName << "': not an object or executable");
  }
  
  m_name = moduleName;
  realpath(m_name);
  
  // -------------------------------------------------------
  // 2. Collect data from BFD
  // -------------------------------------------------------

  // Set flags.  FIXME: both executable and dynamic flags can be set
  // on some architectures (e.g. alpha).
  flagword flags = bfd_get_file_flags(m_bfd);
  if (flags & EXEC_P) {         // BFD is directly executable
    m_type = Executable;
  } 
  else if (flags & DYNAMIC) { // BFD is a dynamic object
    m_type = SharedLibrary;
  } 
  else if (flags) {
    m_type = Unknown;
  }
  
#if defined(HAVE_HPC_GNUBINUTILS)
  if (bfd_get_arch(m_bfd) == bfd_arch_alpha) {
    m_txtBeg = bfd_ecoff_get_text_start(m_bfd);
    m_txtEnd = bfd_ecoff_get_text_end(m_bfd);
    m_begVMA = m_txtBeg;
  } 
  else {
    // Currently, this is ELF specific
    m_txtBeg = bfd_get_start_address(m_bfd); // entry point
    m_begVMA = bfd_get_first_addr(m_bfd);     
  }
#else
  m_txtBeg = bfd_get_start_address(m_bfd); // entry point
  m_begVMA = m_txtBeg;
#endif /* HAVE_HPC_GNUBINUTILS */
  
  // -------------------------------------------------------
  // 3. Configure ISA.  
  // -------------------------------------------------------

  // Create a new ISA (this may not be necessary, but it is cheap)
  ISA* newisa = NULL;
  switch (bfd_get_arch(m_bfd)) {
    case bfd_arch_mips:
      newisa = new MipsISA;
      break;
    case bfd_arch_alpha:
      newisa = new AlphaISA;
      break;
    case bfd_arch_sparc:
      newisa = new SparcISA;
      break;
    case bfd_arch_i386: // x86 and x86_64
      newisa = new x86ISA(bfd_get_mach(m_bfd) == bfd_mach_x86_64);
      break;
    case bfd_arch_ia64:
      newisa = new IA64ISA;
      break;
    default:
      DIAG_Die("Unknown bfd arch: " << bfd_get_arch(m_bfd));
  }

  // Sanity check.  Test to make sure the new LM is using the
  // same ISA type.
  if (!isa) {
    isa = newisa;
  }
  else {
    delete newisa;
    // typeid(*isa).m_name()
    DIAG_Assert(typeid(*newisa) == typeid(*isa),
		"Cannot simultaneously open LMs with different ISAs!");
  }
}


void
binutils::LM::read()
{
  // If the file has not been opened...
  DIAG_Assert(!m_name.empty(), "Must call LM::Open first");

  // Read if we have not already done so
  if (m_bfdSymTbl == NULL
      && m_sections.size() == 0 && m_vmaToInsnMap.size() == 0) {
    ReadSymbolTables();
    ReadSegs();
  } 
}


// relocate: Internally, all operations are performed on non-relocated
// VMAs.  All routines operating on VMAs should call UnRelocateVMA(),
// which will do the right thing.
void 
binutils::LM::relocate(VMA textBegReloc)
{
  DIAG_Assert(m_txtBeg != 0, "LM::Relocate not supported!");
  m_textBegReloc = textBegReloc;
  
  if (m_textBegReloc == 0) {
    m_unRelocDelta = 0;
  } 
  else {
    //m_unRelocDelta = -(m_textBegReloc - m_txtBeg); // FMZ
      m_unRelocDelta = -(m_textBegReloc - m_begVMA);
  } 
}


binutils::Proc*
binutils::LM::findProc(VMA vma) const
{
  VMA unrelocVMA = UnRelocateVMA(vma);
  VMAInterval unrelocInt(unrelocVMA, unrelocVMA + 1); // size must be > 0
  VMAToProcMap::const_iterator it = m_vmaToProcMap.find(unrelocInt);
  Proc* proc = (it != m_vmaToProcMap.end()) ? it->second : NULL;
  return proc;
}


void
binutils::LM::insertProc(VMAInterval vmaint, binutils::Proc* proc)
{
  VMAInterval unrelocInt(UnRelocateVMA(vmaint.beg()), 
			 UnRelocateVMA(vmaint.end()));
  
  // FIXME: It wouldn't hurt to verify this isn't a duplicate
  m_vmaToProcMap.insert(VMAToProcMap::value_type(unrelocInt, proc));
}


MachInsn*
binutils::LM::findMachInsn(VMA vma, ushort &size) const
{
  MachInsn* minsn = NULL;
  size = 0;
  Insn* insn = findInsn(vma, 0);
  if (insn) {
    size  = insn->size();
    minsn = insn->bits();
  }
  return minsn; 
}


binutils::Insn*
binutils::LM::findInsn(VMA vma, ushort opIndex) const
{
  VMA unrelocVMA = UnRelocateVMA(vma);
  VMA mapVMA = isa->ConvertVMAToOpVMA(unrelocVMA, opIndex);
  
  VMAToInsnMap::const_iterator it = m_vmaToInsnMap.find(mapVMA);
  Insn* insn = (it != m_vmaToInsnMap.end()) ? it->second : NULL;
  return insn;
}


void
binutils::LM::insertInsn(VMA vma, ushort opIndex, binutils::Insn* insn)
{
  VMA unrelocVMA = UnRelocateVMA(vma);
  VMA mapVMA = isa->ConvertVMAToOpVMA(unrelocVMA, opIndex);

  // FIXME: It wouldn't hurt to verify this isn't a duplicate
  m_vmaToInsnMap.insert(VMAToInsnMap::value_type(mapVMA, insn));
}


bool
binutils::LM::GetSourceFileInfo(VMA vma, ushort opIndex,
				string& func, 
				string& file, SrcFile::ln& line) /*const*/
{
  bool STATUS = false;
  func = file = "";
  line = 0;

  if (!m_bfdSymTbl) { return STATUS; }
  
  VMA unrelocVMA = UnRelocateVMA(vma);
  VMA opVMA = isa->ConvertVMAToOpVMA(unrelocVMA, opIndex);
  
  // Find the Seg where this vma lives.
  asection *bfdSeg = NULL;
  VMA base = 0;
  for (LMSegIterator it(*this); it.IsValid(); ++it) {
    Seg* sec = it.Current();
    if (sec->IsIn(opVMA)) {
      // Obtain the bfd section corresponding to our Seg.
      bfdSeg = bfd_get_section_by_name(m_bfd, sec->name().c_str());
      base = bfd_section_vma(m_bfd, bfdSeg);
      break; 
    } 
  }
  if (!bfdSeg) {
    return STATUS;
  }

  // Obtain the source line information.
  const char *bfd_func = NULL, *bfd_file = NULL;
  uint bfd_line = 0;

  bfd_boolean fnd = 
    bfd_find_nearest_line(m_bfd, bfdSeg, m_bfdSymTbl,
			  opVMA - base, &bfd_file, &bfd_func, &bfd_line);
  if (fnd) {
    STATUS = (bfd_file && bfd_func && SrcFile::isValid(bfd_line));
    
    if (bfd_func) {
      func = bfd_func;
    }
    if (bfd_file) { 
      file = bfd_file;
      m_realpath_mgr.realpath(file);
    }
    line = (SrcFile::ln)bfd_line;
  }

  return STATUS;
}


bool
binutils::LM::GetSourceFileInfo(VMA begVMA, ushort bOpIndex,
				VMA endVMA, ushort eOpIndex,
				string& func, string& file,
				SrcFile::ln& begLine, SrcFile::ln& endLine,
				unsigned flags) /*const*/
{
  bool STATUS = false;
  func = file = "";
  begLine = endLine = 0;

  // Enforce condition that 'begVMA' <= 'endVMA'. (No need to unrelocate!)
  VMA begOpVMA = isa->ConvertVMAToOpVMA(begVMA, bOpIndex);
  VMA endOpVMA = isa->ConvertVMAToOpVMA(endVMA, eOpIndex);
  if (! (begOpVMA <= endOpVMA) ) {
    VMA tmpVMA = begVMA;       // swap 'begVMA' with 'endVMA'
    begVMA = endVMA; 
    endVMA = tmpVMA;
    ushort tmpOpIdx = bOpIndex; // swap 'bOpIndex' with 'eOpIndex'
    bOpIndex = eOpIndex;
    eOpIndex = tmpOpIdx;
  } 

  // Attempt to find source file info
  string func1, func2, file1, file2;
  bool call1 = GetSourceFileInfo(begVMA, bOpIndex, func1, file1, begLine);
  bool call2 = GetSourceFileInfo(endVMA, eOpIndex, func2, file2, endLine);
  STATUS = (call1 && call2);

  // Error checking and processing: 'func'
  if (!func1.empty() && !func2.empty()) {
    func = func1; // prefer the first call
    if (func1 != func2) {
      STATUS = false; // we are accross two different functions
    }
  } 
  else if (!func1.empty() && func2.empty()) {
    func = func1;
  } 
  else if (func1.empty() && !func2.empty()) {
    func = func2;
  } // else (func1.empty && func2.empty()): use default values

  // Error checking and processing: 'file'
  if (!file1.empty() && !file2.empty()) {
    file = file1; // prefer the first call
    if (file1 != file2) {
      STATUS = false; // we are accross two different files
      endLine = begLine; // 'endLine' makes no sense since we return 'file1'
    }
  } 
  else if (!file1.empty() && file2.empty()) {
    file = file1;
  } 
  else if (file1.empty() && !file2.empty()) {
    file = file2;
  } // else (file1.empty && file2.empty()): use default values

  // Error checking and processing: 'begLine' and 'endLine'
  if (SrcFile::isValid(begLine) && !SrcFile::isValid(endLine)) { 
    endLine = begLine; 
  } 
  else if (SrcFile::isValid(endLine) && !SrcFile::isValid(begLine)) {
    begLine = endLine;
  } 
  else if (flags 
	   && begLine > endLine) { // perhaps due to insn. reordering...
    SrcFile::ln tmp = begLine;     // but unlikely given the way this is
    begLine = endLine;             // typically called
    endLine = tmp;
  }

  return STATUS;
}


void
binutils::LM::textBegEndVMA(VMA* begVMA, VMA* endVMA)
{ 
  VMA curr_begVMA;
  VMA curr_endVMA;
  VMA sml_begVMA = 0;
  VMA lg_endVMA  = 0;
  for (LMSegIterator it(*this); it.IsValid(); ++it) {
    Seg* sec = it.Current(); 
    if (sec->GetType() == sec->Text)  {    
      if (!(sml_begVMA || lg_endVMA)) {
	sml_begVMA = sec->GetBeg();
	lg_endVMA  = sec->GetEnd(); 
      } 
      else { 
	curr_begVMA = sec->GetBeg();
	curr_endVMA = sec->GetEnd(); 
	if (curr_begVMA < sml_begVMA)
	  sml_begVMA = curr_begVMA;
	if (curr_endVMA > lg_endVMA)
	  lg_endVMA = curr_endVMA;
      }
    }
  }   
  
  *begVMA = sml_begVMA;
  *endVMA = lg_endVMA; 
}


string
binutils::LM::toString(int flags, const char* pre) const
{
  std::ostringstream os;
  dump(os, flags, pre);
  return os.str();
}


void
binutils::LM::dump(std::ostream& o, int flags, const char* pre) const
{
  string p(pre);
  string p1 = p;
  string p2 = p1 + "  ";

  o << p << "==================== Load Module Dump ====================\n";

  o << p1 << "BFD version: ";
#ifdef BFD_VERSION
  o << BFD_VERSION << endl;
#else
  o << "-unknown-" << endl;
#endif
  
  o << std::showbase;
  o << p1 << "Load Module Information:\n";
  DumpModuleInfo(o, p2.c_str());

  o << p1 << "Load Module Contents:\n";
  dumpme(o, p2.c_str());

  if (flags & DUMP_Flg_SymTab) {
    o << p2 << "Symbol Table (" << m_numSyms << "):\n";
    DumpSymTab(o, p2.c_str());
  }
  
  o << p2 << "Sections (" << numSegs() << "):\n";
  for (LMSegIterator it(*this); it.IsValid(); ++it) {
    Seg* sec = it.Current();
    sec->dump(o, flags, p2.c_str());
  }
}


void
binutils::LM::ddump() const
{
  dump(std::cerr);
}


void
binutils::LM::dumpme(std::ostream& o, const char* pre) const
{
}


void
binutils::LM::dumpProcMap(std::ostream& os, unsigned flag, 
			  const char* pre) const
{
  for (VMAToProcMap::const_iterator it = m_vmaToProcMap.begin(); 
       it != m_vmaToProcMap.end(); ++it) {
    os << it->first.toString() << " --> " << hex << "Ox" << it->second 
       << dec << endl;
    if (flag != 0) {
      os << it->second->toString();
    }
  }
}


void
binutils::LM::ddumpProcMap(unsigned flag) const
{
  dumpProcMap(std::cerr, flag);
}

//***************************************************************************

int
binutils::LM::SymCmpByVMAFunc(const void* s1, const void* s2)
{
  asymbol *a = (asymbol *)s1;
  asymbol *b = (asymbol *)s2;

  // Primary sort key: Symbol's VMA (ascending).
  if (bfd_asymbol_value(a) < bfd_asymbol_value(b)) {
    return -1;
  } 
  else if (bfd_asymbol_value(a) > bfd_asymbol_value(b)) {
    return 1; 
  } 
  else {
    return 0;
  }
}


bool
binutils::LM::ReadSymbolTables()
{
  bool STATUS = true;

  // First, attempt to get the normal symbol table.  If that fails,
  // attempt to read the dynamic symbol table.
  long bytesNeeded = bfd_get_symtab_upper_bound(m_bfd);
  
  // If we found a populated symbol table...
  if (bytesNeeded > 0) {
    m_bfdSymTbl = new asymbol*[bytesNeeded];
    m_numSyms = bfd_canonicalize_symtab(m_bfd, m_bfdSymTbl);
    if (m_numSyms == 0) {
      delete[] m_bfdSymTbl;
      m_bfdSymTbl = NULL;
    }
  }

  // If we found no symbols, or if there was an error reading symbolic info...
  if (bytesNeeded <= 0 || m_numSyms == 0) {

    // Either there are no symbols or there is no normal symbol table.
    // So try obtaining dynamic symbol table.  Unless this is a mips
    // binary, we emit a user warning.  (For some reason, it seems
    // that that standard mips symbol table is the dynamic one; and
    // don't want this warning emitted for every single mips binary.)

    if (bfd_get_arch(m_bfd) != bfd_arch_mips) {
      DIAG_Msg(2, "'" << name() << "': No regular symbols found; consulting dynamic symbols.");
    }

    bytesNeeded = bfd_get_dynamic_symtab_upper_bound(m_bfd);
    if (bytesNeeded <= 0) {
      // We can't find any symbols. 
      DIAG_Msg(1, "Warning: '" << name() << "': No dynamic symbols found.");
      return false;
    }
    
    m_bfdSymTbl = new asymbol*[bytesNeeded];
    m_numSyms = bfd_canonicalize_dynamic_symtab(m_bfd, m_bfdSymTbl);
  }
  DIAG_Assert(m_numSyms >= 0, "");

  // Make a scratch copy of the symbol table.
  m_bfdSymTblSort = new asymbol*[bytesNeeded];
  memcpy(m_bfdSymTblSort, m_bfdSymTbl, bytesNeeded);

  // Sort scratch symbol table by VMA.
  QuickSort QSort;
  QSort.Create((void **)(m_bfdSymTblSort), LM::SymCmpByVMAFunc);
  QSort.Sort(0, m_numSyms - 1);
  return STATUS;
}


bool
binutils::LM::ReadSegs()
{
  bool STATUS = true;

  // Create sections.
  // Pass symbol table and debug summary information for each section
  // into that section as it is created.
  if (!m_bfdSymTbl) { 
    return false; 
  }

  m_dbgInfo.read(m_bfd, m_bfdSymTbl);

  // Process each section in the object file.
  for (asection *sec = m_bfd->sections; sec; sec = sec->next) {

    // 1. Determine initial section attributes
    string secName(bfd_section_name(m_bfd, sec));
    bfd_vma secBeg = bfd_section_vma(m_bfd, sec);
    uint64_t secSz = bfd_section_size(m_bfd, sec) / bfd_octets_per_byte(m_bfd);
    bfd_vma secEnd = secBeg + secSz;
    
    // 2. Create section
    if (sec->flags & SEC_CODE) {
      TextSeg *newSeg =
	new TextSeg(this, secName, secBeg, secEnd, secSz,
		    m_bfdSymTblSort, m_numSyms, m_bfd);
      insertSeg(newSeg);
    } 
    else {
      Seg *newSeg = new Seg(this, secName, Seg::Data, secBeg, secEnd, secSz);
      insertSeg(newSeg);
    }
  }

  m_dbgInfo.clear();
  
  return STATUS;
}


bool 
binutils::LM::GetProcFirstLineInfo(VMA vma, ushort opIndex, 
				   SrcFile::ln &line) const
{
  bool STATUS = false;
  line = 0;

  VMA unrelocVMA = UnRelocateVMA(vma);
  VMA opVMA = isa->ConvertVMAToOpVMA(unrelocVMA, opIndex);

  VMAInterval vmaint(opVMA, opVMA + 1); // [opVMA, opVMA + 1)

  VMAToProcMap::const_iterator it = m_vmaToProcMap.find(vmaint);
  if (it != m_vmaToProcMap.end()) {
    Proc* proc = it->second;
    line = proc->begLine();
    STATUS = true;
  }
  DIAG_MsgIf(DBG_BLD_PROC_MAP, "LM::GetProcFirstLineInfo " 
	     << vmaint.toString() << " = " << line);

  return STATUS;
}


void
binutils::LM::DumpModuleInfo(std::ostream& o, const char* pre) const
{
  string p(pre);

  o << p << "Name: `" << name() << "'\n";

  o << p << "Format: `" << bfd_get_target(m_bfd) << "'" << endl;
  // bfd_get_flavour

  o << p << "Type: `";
  switch (type()) {
    case Executable:    
      o << "Executable (fully linked except for possible DSOs)'\n";
      break;
    case SharedLibrary: 
      o << "Dynamically Shared Library'\n";
      break;
    case Unknown:
      o << "Unknown load module type'\n";
      break;
    default:
      DIAG_Die("Invalid load module type: " << type());
  }
  
  o << p << "Load VMA: " << hex << m_begVMA << dec << "\n";

  o << p << "Text(beg,end): " 
    << hex << textBeg() << ", " << textEnd() << dec << "\n";
  
  o << p << "Endianness: `"
    << ( (bfd_big_endian(m_bfd)) ? "Big'\n" : "Little'\n" );
  
  o << p << "Architecture: `";
  switch (bfd_get_arch(m_bfd)) {
    case bfd_arch_alpha: o << "Alpha'\n"; break;
    case bfd_arch_mips:  o << "MIPS'\n";  break;
    case bfd_arch_sparc: o << "Sparc'\n"; break;
    case bfd_arch_i386:  o << "x86'\n";   break;
    case bfd_arch_ia64:  o << "IA-64'\n"; break; 
    default: DIAG_Die("Unknown bfd arch: " << bfd_get_arch(m_bfd));
  }

  o << p << "Architectural implementation: `";
  switch (bfd_get_arch(m_bfd)) {
    case bfd_arch_alpha:
      switch (bfd_get_mach(m_bfd)) {
        case bfd_mach_alpha_ev4: o << "EV4'\n"; break;
        case bfd_mach_alpha_ev5: o << "EV5'\n"; break;
        case bfd_mach_alpha_ev6: o << "EV6'\n"; break;
        default:                 o << "-unknown Alpha-'\n"; break;
      }
      break;
    case bfd_arch_mips:
      switch (bfd_get_mach(m_bfd)) {
        case bfd_mach_mips3000:  o << "R3000'\n"; break;
        case bfd_mach_mips4000:  o << "R4000'\n"; break;
        case bfd_mach_mips6000:  o << "R6000'\n"; break;
        case bfd_mach_mips8000:  o << "R8000'\n"; break;
        case bfd_mach_mips10000: o << "R10000'\n"; break;
        case bfd_mach_mips12000: o << "R12000'\n"; break;
        default:                 o << "-unknown MIPS-'\n";
      }
      break;
    case bfd_arch_sparc: 
      switch (bfd_get_mach(m_bfd)) {
        case bfd_mach_sparc_sparclet:     o << "let'\n"; break;
        case bfd_mach_sparc_sparclite:    o << "lite'\n"; break;
        case bfd_mach_sparc_sparclite_le: o << "lite_le'\n"; break;
        case bfd_mach_sparc_v8plus:       o << "v8plus'\n"; break;
        case bfd_mach_sparc_v8plusa:      o << "v8plusa'\n"; break;
        case bfd_mach_sparc_v8plusb:      o << "v8plusb'\n"; break;
        case bfd_mach_sparc_v9:           o << "v9'\n"; break;
        case bfd_mach_sparc_v9a:          o << "v9a'\n"; break;
        case bfd_mach_sparc_v9b:          o << "v9b'\n"; break;
        default:                          o << "-unknown Sparc-'\n";
      }
      break;
    case bfd_arch_i386:
      switch (bfd_get_mach(m_bfd)) {
        case bfd_mach_i386_i386:  o << "x86'\n"; break;
        case bfd_mach_i386_i8086: o << "x86 (8086)'\n"; break;
        case bfd_mach_x86_64:     o << "x86_64'\n"; break;
        default:                  o << "-unknown x86-'\n";
      }
      break;
    case bfd_arch_ia64:
      o << "IA-64'\n"; 
      break; 
    default: 
      DIAG_Die("Unknown bfd arch: " << bfd_get_arch(m_bfd));
  }
  
  o << p << "Bits per byte: "    << bfd_arch_bits_per_byte(m_bfd)    << endl;
  o << p << "Bits per address: " << bfd_arch_bits_per_address(m_bfd) << endl;
  o << p << "Bits per word: "    << m_bfd->arch_info->bits_per_word  << endl;
}


void
binutils::LM::DumpSymTab(std::ostream& o, const char* pre) const
{
  string p(pre);
  string p1 = p + "  ";

  o << p << "------------------- Symbol Table Dump ------------------\n";

  // There is something strange about MIPS/IRIX symbol tables (dynamic).
  // Every function symbol seems to have section *ABS*.  Thus, we
  // can't obtain any relevant section information from the symbol
  // itself.  I haven't noticed this on any other platform.
  for (int i = 0; i < m_numSyms; i++) {
    asymbol *sym = m_bfdSymTblSort[i]; // m_bfdSymTbl[i];
    o << p1 << hex << (bfd_vma)bfd_asymbol_value(sym) << ": " << dec
      << "[" << sym->section->name << "] "
      << ((sym->flags & BSF_FUNCTION) ? " * " : "   ")
      << bfd_asymbol_name(sym) << endl;
  } 
}


//***************************************************************************
// Exe
//***************************************************************************

binutils::Exe::Exe()
  : m_startVMA(0)
{
}


binutils::Exe::~Exe()
{
}


void
binutils::Exe::open(const char* moduleName)
{
  LM::open(moduleName);
  if (type() != LM::Executable) {
    BINUTILS_Throw("'" << moduleName << "' is not an executable.");
  }

  m_startVMA = bfd_get_start_address(m_bfd);
}


void
binutils::Exe::dump(std::ostream& o, int flags, const char* pre) const
{
  LM::dump(o, flags, pre);
}


void
binutils::Exe::dumpme(std::ostream& o, const char* pre) const
{
  o << pre << "Program start address: " << hex << GetStartVMA()
    << dec << endl;
}


//***************************************************************************
// LMSegIterator
//***************************************************************************

binutils::LMSegIterator::LMSegIterator(const LM& _lm)
  : lm(_lm)
{
  Reset();
}

binutils::LMSegIterator::~LMSegIterator()
{
}

//***************************************************************************
