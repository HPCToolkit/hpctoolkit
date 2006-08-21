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
//    LoadModule.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <typeinfo>
#include <string>
using std::string;

//*************************** User Include Files ****************************

#include <include/gnu_bfd.h>

#include "LoadModule.hpp"
#include "Section.hpp"
#include "Instruction.hpp"
#include "Procedure.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/Assertion.h>
#include <lib/support/QuickSort.hpp>
#include <lib/support/Trace.hpp>

#include <lib/isa/MipsISA.hpp>
#include <lib/isa/AlphaISA.hpp>
#include <lib/isa/SparcISA.hpp>
#include <lib/isa/x86ISA.hpp>
#include <lib/isa/IA64ISA.hpp>

//*************************** Forward Declarations **************************

using std::cerr;
using std::endl;
using std::hex;
using std::dec;

//***************************************************************************

//***************************************************************************
// LoadModule
//***************************************************************************

// current ISA (see comments in header)
ISA* LoadModule::isa = NULL;


class LoadModuleImpl {
public:
  asymbol** bfdSymbolTable;        // Unmodified BFD symbol table
  asymbol** sortedSymbolTable;     // Sorted BFD symbol table
  int numSyms;                     // Number of syms in table.
  bfd* abfd;                       // BFD of this module.
};


LoadModule::LoadModule()
  : type(Unknown), textBeg(0), textEnd(0), 
    textBegReloc(0), unRelocDelta(0)
{
  impl = new LoadModuleImpl;
  impl->bfdSymbolTable = impl->sortedSymbolTable = NULL;
  impl->abfd = NULL;
}


LoadModule::~LoadModule()
{
  // Clear impl
  delete[] impl->bfdSymbolTable;    impl->bfdSymbolTable = NULL;
  delete[] impl->sortedSymbolTable; impl->sortedSymbolTable = NULL; 
  if (impl->abfd) {
    bfd_close(impl->abfd);
    impl->abfd = NULL;
  }
  delete impl;

  // Clear sections
  for (LoadModuleSectionIterator it(*this); it.IsValid(); ++it) {
    delete it.Current(); // Section*
  }
  sections.clear();
    
  // Clear vmaToInsMap
  VMAToInsnMap::iterator it;
  for (it = vmaToInsnMap.begin(); it != vmaToInsnMap.end(); ++it) {
    delete (*it).second; // Instruction*
  }
  vmaToInsnMap.clear();

  // reset isa  
  delete isa;
  isa = NULL;
}


bool
LoadModule::Open(const char* moduleName)
{
  IFTRACE << "LoadModule::Open: " << moduleName << endl;

  if (!name.empty()) {
    // 'moduleName' should be equal to what already exists
    DIAG_Assert(name == moduleName, "Cannot open a different file!");
    return true;
  }

  // -------------------------------------------------------
  // 1. Initialize bfd and open the object file.
  // -------------------------------------------------------

  // Determine file existence.
  bfd_init();
  bfd *abfd = bfd_openr(moduleName, "default");
  if (!abfd) {
    cerr << "Error: `" << moduleName << "': " << bfd_errmsg(bfd_get_error())
	 << endl; 
    return false;
  }
  
  // bfd_object: The BFD may contain data, symbols, relocations and
  //   debug info.
  // bfd_archive: The BFD contains other BFDs and an optional index.
  // bfd_core: The BFD contains the result of an executable core dump.
  if (!bfd_check_format(abfd, bfd_object)) {
    cerr << "Error: `" << moduleName << "': not an object or executable\n";
    return false;
  }
  
  name = moduleName;
  impl->abfd = abfd;

  // -------------------------------------------------------
  // 2. Collect data from BFD
  // -------------------------------------------------------

  // Set flags.  FIXME: both executable and dynamic flags can be set
  // on some architectures (e.g. alpha).
  flagword flags = bfd_get_file_flags(impl->abfd);
  if (flags & EXEC_P) {         // BFD is directly executable
    type = Executable;
  } 
  else if (flags & DYNAMIC) { // BFD is a dynamic object
    type = SharedLibrary;
  } 
  else if (flags) {
    type = Unknown;
  }
  
  // FIXME: only do for alpha at the moment 
  if (bfd_get_arch(impl->abfd) == bfd_arch_alpha) {
    textBeg   = bfd_ecoff_get_text_start(impl->abfd);
    textEnd   = bfd_ecoff_get_text_end(impl->abfd);
    firstaddr = textBeg;
  } 
  else {
    textBeg   = bfd_get_start_address(impl->abfd); //this is the entry point
    firstaddr = bfd_get_first_addr(impl->abfd);     
  }
  
  // -------------------------------------------------------
  // 3. Configure ISA.  
  // -------------------------------------------------------

  // Create a new ISA (this may not be necessary, but it is cheap)
  ISA* newisa = NULL;
  switch (bfd_get_arch(impl->abfd)) {
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
      newisa = new x86ISA(bfd_get_mach(impl->abfd) == bfd_mach_x86_64);
      break;
    case bfd_arch_ia64:
      newisa = new IA64ISA;
      break;
    default:
      cerr << "Could not configure ISA." << endl;
      BriefAssertion(false);
  }

  // Sanity check.  Test to make sure the new LoadModule is using the
  // same ISA type.
  if (!isa) {
    isa = newisa;
  }
  else {
    delete newisa;
    DIAG_Assert(typeid(*newisa) == typeid(*isa),
		"Cannot simultaneously open LoadModules with different ISAs!");
  }
  
  return true;
}


bool
LoadModule::Read()
{
  bool STATUS = true;
  // If the file has not been opened...
  DIAG_Assert(!name.empty(), "Must call LoadModule::Open first");

  // Read if we have not already done so
  if (impl->bfdSymbolTable == NULL
      && sections.size() == 0 && vmaToInsnMap.size() == 0) {
    STATUS &= ReadSymbolTables();
    STATUS &= ReadSections();
    buildVMAToProcedureMap();
  } 
  
  return STATUS;
}


// Relocate: Internally, all operations are performed on non-relocated
// VMAs.  All routines operating on VMAs should call UnRelocateVMA(),
// which will do the right thing.
void 
LoadModule::Relocate(VMA textBegReloc_)
{
  BriefAssertion(textBeg != 0 && "Relocation not supported on this arch!");
  textBegReloc = textBegReloc_;
  
  if (textBegReloc == 0) {
    unRelocDelta = 0;
  } 
  else {
    //unRelocDelta = -(textBegReloc - textBeg); // FMZ
      unRelocDelta = -(textBegReloc - firstaddr);
  } 
}

bool 
LoadModule::IsRelocated() const 
{ 
  return (textBegReloc != 0);
}



MachInst*
LoadModule::GetMachInst(VMA vma, ushort &size) const
{
  MachInst* minst = NULL;
  size = 0;
  Instruction* inst = GetInst(vma, 0);
  if (inst) {
    size  = inst->GetSize();
    minst = inst->GetBits();
  }
  return minst; 
}

Instruction*
LoadModule::GetInst(VMA vma, ushort opIndex) const
{
  VMA unrelocVMA = UnRelocateVMA(vma);
  VMA mapVMA = isa->ConvertVMAToOpVMA(unrelocVMA, opIndex);
  
  Instruction* inst = NULL;
  VMAToInsnMap::const_iterator it = vmaToInsnMap.find(mapVMA);
  if (it != vmaToInsnMap.end()) {
    inst = (*it).second;
  }
  return inst;
}

void
LoadModule::AddInst(VMA vma, ushort opIndex, Instruction *inst)
{
  VMA unrelocVMA = UnRelocateVMA(vma);
  VMA mapVMA = isa->ConvertVMAToOpVMA(unrelocVMA, opIndex);

  // FIXME: It wouldn't hurt to verify this isn't a duplicate
  vmaToInsnMap.insert(VMAToInsnMap::value_type(mapVMA, inst));
}


bool
LoadModule::GetSourceFileInfo(VMA vma, ushort opIndex,
			      string& func, string& file, suint& line) const
{
  bool STATUS = false;
  func = file = "";
  line = 0;

  if (!impl->bfdSymbolTable) { return STATUS; }
  
  // This function assumes that the interface type 'suint' can hold
  // the bfd_find_nearest_line line number type.  
  unsigned int bfd_line = 0;
  //BriefAssertion(sizeof(suint) >= sizeof(bfd_line));

  VMA unrelocVMA = UnRelocateVMA(vma);
  VMA opVMA = isa->ConvertVMAToOpVMA(unrelocVMA, opIndex);
  
  // Find the Section where this vma lives.
  asection *bfdSection = NULL;
  VMA base = 0;
  for (LoadModuleSectionIterator it(*this); it.IsValid(); ++it) {
    Section* sec = it.Current();
    if (sec->IsIn(opVMA)) {
      // Obtain the bfd section corresponding to our Section.
      bfdSection = bfd_get_section_by_name(impl->abfd, sec->GetName().c_str());
      base = bfd_section_vma(impl->abfd, bfdSection);
      break; 
    } 
  } 

  // Obtain the source line information.
  const char *_func = NULL, *_file = NULL;
  if (bfdSection
      && bfd_find_nearest_line(impl->abfd, bfdSection, impl->bfdSymbolTable,
			       opVMA - base, &_file, &_func, &bfd_line)) {
    STATUS = (_file && _func && IsValidLine(bfd_line));
    if (_func) { func = _func; }
    if (_file) { file = _file; }
    line = bfd_line;
  }

  return STATUS;
}

bool
LoadModule::GetSourceFileInfo(VMA begVMA, ushort bOpIndex,
			      VMA endVMA, ushort eOpIndex,
			      string& func, string& file,
			      suint &begLine, suint &endLine) const
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
  if (IsValidLine(begLine) && !IsValidLine(endLine)) { 
    endLine = begLine; 
  } 
  else if (IsValidLine(endLine) && !IsValidLine(begLine)) {
    begLine = endLine;
  } 
  else if (begLine > endLine) { // perhaps due to inst. reordering...
    suint tmp = begLine;          // but unlikely given the way this is
    begLine = endLine;            // typically called
    endLine = tmp;
  }

  return STATUS;
}


void
LoadModule::GetTextBegEndVMA(VMA* begVMA, VMA* endVMA)
{ 
  VMA curr_begVMA;
  VMA curr_endVMA;
  VMA sml_begVMA = 0;
  VMA lg_endVMA  = 0;
  for (LoadModuleSectionIterator it(*this); it.IsValid(); ++it) {
    Section* sec = it.Current(); 
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


void
LoadModule::Dump(std::ostream& o, const char* pre) const
{
  string p(pre);
  string p1 = p + "  ";
  string p2 = p1 + "  ";

  o << p << "==================== Load Module Dump ====================\n";

  o << p1 << "BFD version: ";
#ifdef BFD_VERSION
  o << BFD_VERSION << endl;
#else
  o << "-unknown-" << endl;
#endif

  o << p1 << "Load Module Information:\n";
  DumpModuleInfo(o, p2.c_str());

  o << p1 << "Load Module Contents:\n";
  DumpSelf(o, p2.c_str());
  
  IFDOTRACE { // For debugging
    o << p2 << "Symbol Table (" << impl->numSyms << "):\n";
    DumpSymTab(o, p2.c_str());
  }
  
  o << p2 << "Sections (" << GetNumSections() << "):\n";
  for (LoadModuleSectionIterator it(*this); it.IsValid(); ++it) {
    Section* sec = it.Current();
    sec->Dump(o, p2.c_str());
  }
}

void
LoadModule::DDump() const
{
  Dump(std::cerr);
}

void
LoadModule::DumpSelf(std::ostream& o, const char* pre) const
{
}


//***************************   private members    ***************************

int
LoadModule::SymCmpByVMAFunc(const void* s1, const void* s2)
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
LoadModule::ReadSymbolTables()
{
  bool STATUS = true;

  // First, attempt to get the normal symbol table.  If that fails,
  // attempt to read the dynamic symbol table.
  long numSyms = -1;
  long bytesNeeded = bfd_get_symtab_upper_bound(impl->abfd);
  
  // If we found a populated symbol table...
  if (bytesNeeded > 0) {
    impl->bfdSymbolTable = new asymbol*[bytesNeeded];
    numSyms = bfd_canonicalize_symtab(impl->abfd, impl->bfdSymbolTable);
    if (numSyms == 0) {
      delete [] impl->bfdSymbolTable;
      impl->bfdSymbolTable = NULL;
    }
  }

  // If we found no symbols, or if there was an error reading symbolic info...
  if (bytesNeeded <= 0 || numSyms == 0) {

    // Either there are no symbols or there is no normal symbol table.
    // So try obtaining dynamic symbol table.  Unless this is a mips
    // binary, we emit a user warning.  (For some reason, it seems
    // that that standard mips symbol table is the dynamic one; and
    // don't want this warning emitted for every single mips binary.)

    if (bfd_get_arch(impl->abfd) != bfd_arch_mips) {
      cerr << "Warning: `" << GetName() << "': No regular symbols found;"
	   << " will look for dynamic symbols.\n";
      cerr.flush();
    }

    bytesNeeded = bfd_get_dynamic_symtab_upper_bound(impl->abfd);
    if (bytesNeeded <= 0) {
      // We can't find any symbols. 
      cerr << "Warning: `" << GetName() << "': No dynamic symbols found.\n";
      cerr.flush();      
      return false;
    }
    
    impl->bfdSymbolTable = new asymbol*[bytesNeeded];
    numSyms = bfd_canonicalize_dynamic_symtab(impl->abfd,
					      impl->bfdSymbolTable);
  }
  BriefAssertion(numSyms >= 0);
  impl->numSyms = numSyms;

  // Make a scratch copy of the symbol table.
  impl->sortedSymbolTable = new asymbol*[bytesNeeded];
  memcpy(impl->sortedSymbolTable, impl->bfdSymbolTable, bytesNeeded);

  // Sort scratch symbol table by VMA.
  QuickSort QSort;
  QSort.Create((void **)(impl->sortedSymbolTable),
               &(LoadModule::SymCmpByVMAFunc));
  QSort.Sort(0, numSyms - 1);
  return STATUS;
}


bool
LoadModule::ReadSections()
{
  bool STATUS = true;

  // Create sections.
  // Pass symbol table and debug summary information for each section
  // into that section as it is created.
  if (!impl->bfdSymbolTable) { return false; }

  ReadDebugFunctionSummaryInfo();

  // Process each section in the object file.
  bfd *abfd = impl->abfd;
  for (asection *sec = abfd->sections; sec; sec = sec->next) {

    // 1. Determine initial section attributes
    string secName(bfd_section_name(abfd, sec));
    bfd_vma secBeg = bfd_section_vma(abfd, sec);
    suint secSize = bfd_section_size(abfd, sec) / bfd_octets_per_byte(abfd);
    bfd_vma secEnd = secBeg + secSize;
    
    // 2. Create section
    if (sec->flags & SEC_CODE) {
      TextSection *newSection =
	new TextSection(this, secName, secBeg, secEnd, secSize,
			impl->sortedSymbolTable, impl->numSyms, abfd);
      AddSection(newSection);
    } 
    else {
      Section *newSection = new Section(this, secName, Section::Data,
					secBeg, secEnd, secSize);
      AddSection(newSection);
    }
  }

  ClearDebugFunctionSummaryInfo();
  
  return STATUS;
}

// FIXME: replace with diagnostics stuff
#define xDEBUG( flag, code) {if (flag) {code; fflush (stderr); fflush(stdout);}} 
#define DEB_BUILD_PROC_MAP 0

// Builds the map from <proc beg addr, proc end addr> pairs to 
// procedure first line.
bool 
LoadModule::buildVMAToProcedureMap() 
{
  bool STATUS = false;
  for (LoadModuleSectionIterator it(*this); it.IsValid(); ++it) {
    Section* sec = it.Current();
    if (sec->GetType() != Section::Text) {
      continue;
    }
    
    // We have a TextSection.  Iterate over procedures.
    // Obtain the bfd section corresponding to our Section.
    asection *bfdSection = 
      bfd_get_section_by_name(impl->abfd, sec->GetName().c_str());
    VMA base = bfd_section_vma(impl->abfd, bfdSection);
    
    TextSection* tsec = dynamic_cast<TextSection*>(sec);
    for (TextSectionProcedureIterator it1(*tsec); it1.IsValid(); ++it1) {
      Procedure* p = it1.Current();
      VMA begVMA = p->GetBegVMA();
      VMA endVMA = begVMA + p->GetSize();
      suint begLine = p->GetBegLine();
      
      if (!IsValidLine(begLine)) {
	string func, file;
	GetSourceFileInfo(begVMA, 0, func, file, begLine);
      }
      
      vmaToProcMap.insert(VMAToProcedureMap::value_type( VMAInterval(begVMA, endVMA), begLine));
      xDEBUG(DEB_BUILD_PROC_MAP, 
	     fprintf(stderr, "adding procedure %s [%x,%x] beg line %d\n", 
		     p->GetName().c_str(), begVMA, endVMA, begLine););
    }
  } 
  return STATUS;
}


bool 
LoadModule::GetProcedureFirstLineInfo(VMA vma, ushort opIndex, suint &line) 
{
  bool STATUS = false;

  VMA unrelocVMA = UnRelocateVMA(vma);
  VMA opVMA = isa->ConvertVMAToOpVMA(unrelocVMA, opIndex);

  xDEBUG( DEB_BUILD_PROC_MAP,
	  fprintf(stderr, "LoadModule::GetProcedureFirstLineInfo %p %x (%x,%x) unrelocVMA=%x opVMA=%x\n", 
		  this, this, vma, (unsigned) opIndex, unrelocVMA, opVMA););

  VMAToProcedureMap::iterator it = vmaToProcMap.lower_bound( VMAInterval(opVMA,opVMA));
  if (it != vmaToProcMap.end()) {
    it--; // move to predecessor
    line = it->second;
    xDEBUG( DEB_BUILD_PROC_MAP,
	    fprintf(stderr, "LoadModule::GetProcedureFirstLineInfo %p %x (%x,%x) unrelocVMA=%x opVMA=%x found line %d\n", 
		    this, this, vma, (unsigned) opIndex, unrelocVMA, opVMA, line););
    
    STATUS = true;
  } 
  else {
    line = 0;
    xDEBUG( DEB_BUILD_PROC_MAP,
	    fprintf(stderr, "LoadModule::GetProcedureFirstLineInfo %p %x (%x,%x) unrelocVMA=%x opVMA=%x line not found\n", 
		    this, this, vma, (unsigned) opIndex, unrelocVMA, opVMA, line););
    
  }
  return STATUS;
}


bool
LoadModule::ReadDebugFunctionSummaryInfo()
{
  bool STATUS = true;

  if (!impl->bfdSymbolTable) { return false; }
  
  bfd *abfd = impl->abfd;  
  
  // Construct dbgSummary: Currently we only know about ELF/DWARF2
  if (bfd_get_flavour(abfd) == bfd_target_elf_flavour) {
    bfd_forall_dbg_funcinfo_fn_t callback = 
      (bfd_forall_dbg_funcinfo_fn_t)bfd_DbgFuncinfoCallback;
    int cnt = bfd_elf_forall_dbg_funcinfo(abfd, impl->bfdSymbolTable,
					  callback, this);
  }
  
  // Post-process dbgSummary and set parent pointers
  dbgSummary.setParentPointers();

  //dbgSummary.dump(std::cout);
  
  return STATUS;
}


void
LoadModule::ClearDebugFunctionSummaryInfo()
{
  dbgSummary.clear();
}


// Should have function type of 'bfd_forall_dbg_funcinfo_fn_t'
int 
LoadModule::bfd_DbgFuncinfoCallback(void* callback_obj, 
				    void* parent, void* funcinfo)
{
  LoadModule* lm = (LoadModule*)callback_obj;
  
  LoadModule::DbgFuncSummary::Info* finfo = 
    new LoadModule::DbgFuncSummary::Info;

  // Collect information for 'funcinfo'
  bfd_vma begVMA, endVMA;
  const char* name, *filenm;
  unsigned int begLine;
  bfd_forall_dbg_funcinfo_get_decl_info(funcinfo, &name, &filenm, &begLine);
  bfd_forall_dbg_funcinfo_get_vma_bounds(funcinfo, &begVMA, &endVMA);
  if (!name)   { name = ""; }
  if (!filenm) { filenm = ""; }
  
  finfo->begVMA = begVMA;
  finfo->endVMA = endVMA;
  finfo->name = name;
  finfo->filenm = filenm;
  finfo->begLine = (suint)begLine;

  // We are not guaranteed to see parent routines before children.
  // Save begVMA for future processing.
  begVMA = endVMA = 0;
  if (parent) {
    bfd_forall_dbg_funcinfo_get_vma_bounds(parent, &begVMA, &endVMA);
  }
  finfo->parentVMA = begVMA;

  // For now, we assume that if we don't know an address for
  // 'funcinfo', then we can't use it
  if (finfo->begVMA == 0) {
    delete finfo;
    return 0;
  }
  
  std::pair<LoadModule::DbgFuncSummary::iterator, bool> res = 
    lm->dbgSummary.insert(std::make_pair(finfo->begVMA, finfo));
  if (!res.second) {
    // There are two descriptors for the same function.  I don't think
    // this should happen in practice...
    // finfo->dump(std::cout);
    delete finfo;    
  }

  return 0;
}


void
LoadModule::DumpModuleInfo(std::ostream& o, const char* pre) const
{
  string p(pre);
  bfd *abfd = impl->abfd;

  o << p << "Name: `" << GetName() << "'\n";

  o << p << "Format: `" << bfd_get_target(abfd) << "'" << endl;
  // bfd_get_flavour

  o << p << "Type: `";
  switch (GetType()) {
    case Executable:    
      o << "Executable (fully linked except for possible DSOs)'\n";
      break;
    case SharedLibrary: 
      o << "Dynamically Shared Library'\n";
      break;
    default:            
      o << "-unknown-'\n";
      BriefAssertion(false); 
  }
  
  o << p << "Load VMA: " << hex << "0x" << firstaddr << dec << "\n";

  o << p << "Text(beg,end): 0x" << hex << GetTextBeg() << ", 0x"
    << GetTextEnd() << dec << "\n";
  
  o << p << "Endianness: `"
    << ( (bfd_big_endian(abfd)) ? "Big'\n" : "Little'\n" );
  
  o << p << "Architecture: `";
  switch (bfd_get_arch(abfd)) {
    case bfd_arch_alpha: o << "Alpha'\n"; break;
    case bfd_arch_mips:  o << "MIPS'\n";  break;
    case bfd_arch_sparc: o << "Sparc'\n"; break;
    case bfd_arch_i386:  o << "x86'\n";   break;
    case bfd_arch_ia64:  o << "IA-64'\n"; break; 
    default:             o << "-unknown-'\n";   BriefAssertion(false);
  }

  o << p << "Architectural implementation: `";
  switch (bfd_get_arch(abfd)) {
    case bfd_arch_alpha:
      switch (bfd_get_mach(abfd)) {
        case bfd_mach_alpha_ev4: o << "EV4'\n"; break;
        case bfd_mach_alpha_ev5: o << "EV5'\n"; break;
        case bfd_mach_alpha_ev6: o << "EV6'\n"; break;
        default:                 o << "-unknown Alpha-'\n"; break;
      }
      break;
    case bfd_arch_mips:
      switch (bfd_get_mach(abfd)) {
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
      switch (bfd_get_mach(abfd)) {
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
      switch (bfd_get_mach(abfd)) {
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
      o << "-unknown-'\n"; 
      BriefAssertion(false);
  }
  
  o << p << "Bits per byte: "    << bfd_arch_bits_per_byte(abfd)    << endl;
  o << p << "Bits per address: " << bfd_arch_bits_per_address(abfd) << endl;
  o << p << "Bits per word: "    << abfd->arch_info->bits_per_word  << endl;
}


void
LoadModule::DumpSymTab(std::ostream& o, const char* pre) const
{
  string p(pre);
  string p1 = p + "  ";

  o << p << "------------------- Symbol Table Dump ------------------\n";

  // There is something strange about MIPS/IRIX symbol tables (dynamic).
  // Every function symbol seems to have section *ABS*.  Thus, we
  // can't obtain any relevant section information from the symbol
  // itself.  I haven't noticed this on any other platform.
  for (int i = 0; i < impl->numSyms; i++) {
    asymbol *sym = impl->sortedSymbolTable[i]; // impl->bfdSymbolTable[i];
    o << p1 << hex << "0x" << (bfd_vma)bfd_asymbol_value(sym) << ": " << dec
      << "[" << sym->section->name << "] "
      << ((sym->flags & BSF_FUNCTION) ? " * " : "   ")
      << bfd_asymbol_name(sym) << endl;
  } 
}


//***************************************************************************
// DbgFuncSummary
//***************************************************************************

std::ostream&
LoadModule::DbgFuncSummary::Info::dump(std::ostream& os) const
{
  os << "{ DbgFuncSummary::Info: \n";
  os << "  " << name << " [" << hex << begVMA << "-" << endVMA << dec << "]"
     << " parentVMA: " << hex << parentVMA << " --> " << parent << dec << "\n";
  os << "  " << filenm << ":" << begLine << "\n";
  os << "}\n";
  os.flush();
  return os;
}


LoadModule::DbgFuncSummary::DbgFuncSummary()
{
}


LoadModule::DbgFuncSummary::~DbgFuncSummary()
{
  clear();
}

void 
LoadModule::DbgFuncSummary::setParentPointers()
{
  // Set parent pointers assuming begVMA has been set.
  for (const_iterator it = this->begin(); it != this->end(); ++it) {
    Info* x = it->second;
    if (x->parentVMA != 0) {
      x->parent = (*this)[x->parentVMA];
    }
  }
}

void
LoadModule::DbgFuncSummary::clear()
{
  for (const_iterator it = this->begin(); it != this->end(); ++it) {
    delete it->second;
  }
  mMap.clear();
}


std::ostream&
LoadModule::DbgFuncSummary::dump(std::ostream& os) const
{
  os << "{ DbgFuncSummary: \n";
  for (const_iterator it = this->begin(); it != this->end(); ++it) {
    //os << "(" << it->first << " --> " << it->second << ")\n";
    it->second->dump(os);
  }
  os << "}\n";
  os.flush();
  return os;
}

//***************************************************************************
// Executable
//***************************************************************************

Executable::Executable()
  : startVMA(0)
{
}


Executable::~Executable()
{
}


bool
Executable::Open(const char* moduleName)
{
  bool STATUS = LoadModule::Open(moduleName);
  if (STATUS) {
    if (GetType() != LoadModule::Executable) {
      cerr << "Error: `" << moduleName << "' is not an executable.\n";
      return false;
    }

    startVMA = bfd_get_start_address(impl->abfd);
  }
  return STATUS;
}


void
Executable::Dump(std::ostream& o, const char* pre) const
{
  LoadModule::Dump(o);  
}


void
Executable::DDump() const
{
  Dump(std::cerr);
}


void
Executable::DumpSelf(std::ostream& o, const char* pre) const
{
  o << pre << "Program start address: 0x" << hex << GetStartVMA()
    << dec << endl;
}

//***************************************************************************
// LoadModuleSectionIterator
//***************************************************************************

LoadModuleSectionIterator::LoadModuleSectionIterator(const LoadModule& _lm)
  : lm(_lm)
{
  Reset();
}

LoadModuleSectionIterator::~LoadModuleSectionIterator()
{
}

//***************************************************************************
