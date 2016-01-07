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

#include <typeinfo>

#include <string>
using std::string;

#include <sstream>

#include <iostream>
#include <iomanip>
using std::cerr;
using std::endl;

#include <cstring>

//*************************** User Include Files ****************************

#include <include/hpctoolkit-config.h>
#include <include/gcc-attr.h>
#include <include/uint.h>

#include "LM.hpp"
#include "Seg.hpp"
#include "Insn.hpp"
#include "Proc.hpp"

#include "Dbg-LM.hpp"

#include <lib/isa/AlphaISA.hpp>
#include <lib/isa/IA64ISA.hpp>
#include <lib/isa/MipsISA.hpp>
#include <lib/isa/PowerISA.hpp>
#include <lib/isa/SparcISA.hpp>
#include <lib/isa/x86ISA.hpp>
#include <lib/isa/x86ISABinutils.hpp>
#include <lib/isa/x86ISAXed.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>
#include <lib/support/QuickSort.hpp>



//***************************************************************************
// macros
//***************************************************************************

#define DBG_BLD_PROC_MAP 0

#define NORETURNS_DISABLE 0
#define NORETURNS_DEBUG 0
#define NORETURNS_RESULT_NOISY 0
#define NORETURNS_LOOKUP_NOISY 0 
#define NORETURNS_LOOKUP_LOCAL_NOISY 0



//***************************************************************************
// private data 
//***************************************************************************

static const char *noreturn_table[] = {
  // include machine-generated file containing names of functions 
  // that don't return
#include "names.cpp"
};



//***************************************************************************
// type declarations
//***************************************************************************

namespace BinUtil {

class cstring_compare {
public:
  bool operator()(const char *s1, const char *s2) const {
      int result = strcmp(s1,s2);
      return result < 0 ? true : false;
  }
};


class name_set : public std::set<const char*, cstring_compare> {
public:
  name_set() { 
    unsigned int i; 
    for(i = 0; i < sizeof(noreturn_table)/sizeof(char*); i++) 
      insert(noreturn_table[i]); 
  }

  void 
  dumpnames() {  
    for(name_set::iterator i = this->begin(); 
      i != this->end(); i++) {     
      std::cout << *i << std::endl;  
    }
  }
};


name_set noreturn_fn_names; 


class NoReturns : private std::set<VMA> {
public:
  
  NoReturns() { 
  };

  void
  addSynSymEntries(asymbol* syms, long symcount) { 
#if NORETURNS_RESULT_NOISY
    printf("-------- dyn sym noreturn functions ------ \n");
#endif
    //-------------------------------------------------------------------
    // iterate through the functions referenced through the load module's       
    // PLT to identify which of them are known to not return. for each 
    // PLT entry for a function that doesn't return, record the address 
    // of its PLT trampoline so that we can identify calls to functions 
    // that don't return when building a control flow graph.
    //
    // Note: the binutils interface to symbols in the PLT processes 
    //       the symbols and converts each symbol with name "symname" to 
    //       have name "symname@plt". Here, we strip off the suffix
    //       that begins with the @ before comparing a PLT symbol name
    //       with names of functions known to not return.
    //-------------------------------------------------------------------
    asymbol *symbol = syms;
    for (long i = 0; i < symcount; i++, symbol++) {
       //-----------------------------------------------------
       // create a copy of the symbol name that we can modify
       //-----------------------------------------------------
       char *name = strdup(bfd_asymbol_name(symbol)); 

       char *index = strchr(name,'@'); // look for an @ in the symbol
       if (index) {                     // if an @ was found
         *index = 0;                    // remove the suffix @...
         unsigned long addr = bfd_asymbol_value(symbol);
         //---------------------------------------------------------
         // look for the symbol name in the set of functions known to 
         // not return. if it is found, note the address of the PLT
         // entry as the address of a callee that won't return.
         //---------------------------------------------------------
#if NORETURNS_LOOKUP_NOISY
         std::cout << "looking up " << name << " @ " << std::hex << "0x" << addr << std::endl;
#endif
         addIfNoReturn(name, addr);
       }
       free(name);
    }
  };


  void
  addSymEntries(asymbol** syms, long symcount) { 
#if NORETURNS_RESULT_NOISY
    printf("-------- sym noreturn functions ------ \n");
#endif
    //-------------------------------------------------------------------
    // iterate through functions statically linked in the load module
    // to identify which of them are known to not return. for each 
    // function that doesn't return, record its address so that we can 
    // identify calls to functions that don't return when building a 
    // control flow graph.
    //-------------------------------------------------------------------
    asymbol **symbol = syms;
    if (symbol && symcount > 0) {
      for (long i = 0; symbol[i]; i++) {
        const char *name = bfd_asymbol_name(symbol[i]);
        if (symbol[i]->flags & (BSF_FUNCTION))  {
          unsigned long addr = bfd_asymbol_value(symbol[i]);
#if NORETURNS_LOOKUP_NOISY
          std::cout << "looking up " << name << " @ " << std::hex << "0x" 
                    << addr << std::endl;
#endif
          if (addr) {
            addIfNoReturn(name, addr);
          }
        } else if (symbol[i]->flags == BSF_LOCAL)  {
          // look for plt_call and long branch trampolines found in 
          // Power binaries
#if NORETURNS_LOOKUP_LOCAL_NOISY
          unsigned long laddr = bfd_asymbol_value(symbol[i]);
          std::cout << "looking up local symbol " << name << std::hex << "0x" 
                    << laddr << std::endl;
#endif
          if (strstr(name, "long_branch_r2off")) {
            unsigned long addr = bfd_asymbol_value(symbol[i]);
            if (addr) {
              char *dname = strdup(name);
              char *index = strchr(dname,'+'); // look for an @ in the symbol
              if (index) {                     // if an @ was found
                *index = 0;                    // remove the suffix @...
              }
              // on power architectures, need to eliminate 
              // *.long_branch_r2off. prefix
              char *name = dname;
              char *name_suffix = strrchr(dname,'.');
              if (name_suffix) {
                name = name_suffix + 1;
              }
#if NORETURNS_LOOKUP_NOISY
          std::cout << "looking up " << name << " @ " << std::hex << "0x" 
                    << addr << std::endl;
#endif
              addIfNoReturn(name, addr);
              free(dname);
            }
	  } else if (strstr(name, "plt_call")) {
            unsigned long addr = bfd_asymbol_value(symbol[i]);
            if (addr) {
              char *dname = strdup(name);
              char *index = strchr(dname,'@'); // look for an @ in the symbol
              if (index) {                     // if an @ was found
                *index = 0;                    // remove the suffix @...
              }
              // on power architectures, need to eliminate *.plt_call. prefix
              char *name = dname;
              char *name_suffix = strrchr(dname,'.');
              if (name_suffix) {
                name = name_suffix + 1;
              }
#if NORETURNS_LOOKUP_NOISY
          std::cout << "looking up " << name << " @ " << std::hex << "0x" 
                    << addr << std::endl;
#endif
              addIfNoReturn(name, addr);
              free(dname);
            }
	  }
        }
      }
    }
  };

  ~NoReturns() { 
  };

  bool 
  isNoReturn(VMA addr) {
    return NORETURNS_DISABLE ? false : this->find(addr) != this->end();   
  };


  void
  addIfNoReturn(const char *name, uint64_t addr) {
    if (noreturn_fn_names.find(name) != noreturn_fn_names.end()) {
      this->insert(addr);
#if NORETURNS_RESULT_NOISY
      std::cout << name << " @ " << std::hex << "0x" << addr << std::endl;
#endif
    }
  };


  void 
  dump() {  
    std::cout << "noreturns (addresses of functions that don't return)" 
              << std::endl;  
    for(std::set<VMA>::iterator i = this->begin(); i != this->end(); i++) {     
      std::cout << std::hex << "0x" << *i << std::endl;  
    }
  };
};

}


//*************************** Forward Declarations **************************


static void
dumpSymFlag(std::ostream& o, asymbol* sym, int flag, const char* txt, bool& hasPrinted);

//***************************************************************************


//***************************************************************************
// LM
//***************************************************************************

// current ISA (see comments in header)
ISA* BinUtil::LM::isa = NULL;


BinUtil::LM::LM(bool useBinutils)
  : m_type(TypeNULL), m_readFlags(ReadFlg_NULL),
    m_txtBeg(0), m_txtEnd(0), m_begVMA(0),
    m_textBegReloc(0), m_unrelocDelta(0),
    m_bfd(NULL), m_bfdSymTab(NULL), 
    m_bfdDynSymTab(NULL), m_bfdSynthTab(NULL),
    m_bfdSymTabSort(NULL), m_bfdSymTabSz(0), m_bfdDynSymTabSz(0),
    m_bfdSymTabSortSz(0), m_bfdSynthTabSz(0), m_noreturns(0), 
    m_realpathMgr(RealPathMgr::singleton()), m_useBinutils(useBinutils)
{
}


BinUtil::LM::~LM()
{
  for (SegMap::iterator it = m_segMap.begin(); it != m_segMap.end(); ++it) {
    delete it->second; // Seg*
  }
  m_segMap.clear();

  for (InsnMap::iterator it = m_insnMap.begin(); it != m_insnMap.end(); ++it) {
    delete it->second; // Insn*
  }
  m_insnMap.clear();

  // BFD info
  if (m_bfd) {
    bfd_close(m_bfd);
    m_bfd = NULL;
  }

  delete[] m_bfdSymTab;
  m_bfdSymTab = NULL;

  free(m_bfdSynthTab);
  m_bfdSynthTab = NULL;

  delete[] m_bfdSymTabSort;
  m_bfdSymTabSort = NULL; 

  m_bfdSymTabSz = 0;
  m_bfdSymTabSortSz = 0;
  m_bfdSynthTabSz = 0;
  
  // reset isa
  delete isa;
  isa = NULL;

  delete m_noreturns;
  m_noreturns = NULL;
}


void
BinUtil::LM::open(const char* filenm)
{
  DIAG_Assert(Logic::implies(!m_name.empty(), m_name.c_str() == filenm), "Cannot open a different file!");
  
  // -------------------------------------------------------
  // 1. Initialize bfd and open the object file.
  // -------------------------------------------------------

  // Determine file existence.
  bfd_init();
  m_bfd = bfd_openr(filenm, "default");
  if (!m_bfd) {
    BINUTIL_Throw("'" << filenm << "': " << bfd_errmsg(bfd_get_error()));
  }
  
  // bfd_object:  may contain data, symbols, relocations and debug info
  // bfd_archive: contains other BFDs and an optional index
  // bfd_core:    contains the result of an executable core dump
  if (!bfd_check_format(m_bfd, bfd_object)) {
    BINUTIL_Throw("'" << filenm << "': not an object or executable");
  }
  
  m_name = filenm;
  m_realpathMgr.realpath(m_name);
  
  // -------------------------------------------------------
  // 2. Collect data from BFD
  // -------------------------------------------------------

  // Set flags.  FIXME: both executable and dynamic flags can be set
  // on some architectures (e.g. alpha).
  flagword flags = bfd_get_file_flags(m_bfd);
  if (flags & EXEC_P) {         // BFD is directly executable
    m_type = TypeExe;
  } 
  else if (flags & DYNAMIC) { // BFD is a dynamic object
    m_type = TypeDSO;
  } 
  else {
    m_type = TypeNULL;
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
    case bfd_arch_alpha:
      newisa = new AlphaISA;
      break;
    case bfd_arch_i386: // x86 and x86_64
      if (m_useBinutils) {
        newisa = new x86ISABinutils(bfd_get_mach(m_bfd) == bfd_mach_x86_64);
      } else {
        newisa = new x86ISAXed(bfd_get_mach(m_bfd) == bfd_mach_x86_64);
      }
      break;
    case bfd_arch_ia64:
      newisa = new IA64ISA;
      break;
#ifdef bfd_mach_k1om
    case bfd_arch_k1om: // Intel MIC, 64-bit only
      if (m_useBinutils) {
         newisa = new x86ISABinutils(true);
      } else {
         newisa = new x86ISAXed(true);
      }
      break;
#endif
    case bfd_arch_mips:
      newisa = new MipsISA;
      break;
    case bfd_arch_powerpc:
      newisa = new PowerISA;
      break;
    case bfd_arch_sparc:
      newisa = new SparcISA;
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
BinUtil::LM::read(LM::ReadFlg readflg)
{
  // Internal sanity check.
  DIAG_Assert(!m_name.empty(), "Must call LM::Open first");

  m_readFlags = (ReadFlg)(readflg | LM::ReadFlg_fSeg); // enforce ReadFlg rules

  readSymbolTables();
  readSegs();
  computeNoReturns();
}



// relocate: Internally, all operations are performed on non-relocated
// VMAs.  All routines operating on VMAs should call unrelocate(),
// which will do the right thing.
void 
BinUtil::LM::relocate(VMA textBegReloc)
{
  DIAG_Assert(m_txtBeg != 0, "LM::Relocate not supported!");
  m_textBegReloc = textBegReloc;
  
  if (m_textBegReloc == 0) {
    m_unrelocDelta = 0;
  } 
  else {
    //m_unrelocDelta = -(m_textBegReloc - m_txtBeg); // FMZ
      m_unrelocDelta = -(m_textBegReloc - m_begVMA);
  } 
}


MachInsn*
BinUtil::LM::findMachInsn(VMA vma, ushort &size) const
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


bool
BinUtil::LM::findSrcCodeInfo(VMA vma, ushort opIndex,
			     string& func,
			     string& file, SrcFile::ln& line) /*const*/
{
  bool STATUS = false;
  func = file = "";
  line = 0;

  if (!m_bfdSymTab) { 
    return STATUS; 
  }
  
  VMA unrelocVMA = unrelocate(vma);
  VMA opVMA = isa->convertVMAToOpVMA(unrelocVMA, opIndex);
  
  // Find the Seg where this vma lives.
  asection* bfdSeg = NULL;
  VMA base = 0;

  Seg* seg = findSeg(opVMA);
  if (seg) {
    bfdSeg = bfd_get_section_by_name(m_bfd, seg->name().c_str());
    base = bfd_section_vma(m_bfd, bfdSeg);
  }

  if (!bfdSeg) {
    return STATUS;
  }

  // Obtain the source line information.
  const char *bfd_func = NULL, *bfd_file = NULL;
  uint bfd_line = 0;
  bfd_boolean fnd = 
    bfd_find_nearest_line(m_bfd, bfdSeg, m_bfdSymTab,
			  opVMA - base, &bfd_file, &bfd_func, &bfd_line);
  if (fnd) {
    STATUS = (bfd_file && bfd_func && SrcFile::isValid(bfd_line));
    
    if (bfd_func) {
      func = bfd_func;
    }
    if (bfd_file) { 
      file = bfd_file;
      m_realpathMgr.realpath(file);
    }
    line = (SrcFile::ln)bfd_line;
  }

  return STATUS;
}


bool
BinUtil::LM::findSrcCodeInfo(VMA begVMA, ushort bOpIndex,
			     VMA endVMA, ushort eOpIndex,
			     string& func, string& file,
			     SrcFile::ln& begLine, SrcFile::ln& endLine,
			     unsigned flags) /*const*/
{
  bool STATUS = false;
  func = file = "";
  begLine = endLine = 0;

  // Enforce condition that 'begVMA' <= 'endVMA'. (No need to unrelocate!)
  VMA begOpVMA = isa->convertVMAToOpVMA(begVMA, bOpIndex);
  VMA endOpVMA = isa->convertVMAToOpVMA(endVMA, eOpIndex);
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
  bool call1 = findSrcCodeInfo(begVMA, bOpIndex, func1, file1, begLine);
  bool call2 = findSrcCodeInfo(endVMA, eOpIndex, func2, file2, endLine);
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


bool 
BinUtil::LM::findProcSrcCodeInfo(VMA vma, ushort opIndex, 
				 SrcFile::ln &line) const
{
  bool isfound = false;
  line = 0;

  VMA vma_ur = unrelocate(vma);
  VMA opVMA = isa->convertVMAToOpVMA(vma_ur, opIndex);

  VMAInterval ival(opVMA, opVMA + 1); // [opVMA, opVMA + 1)

  ProcMap::const_iterator it = m_procMap.find(ival);
  if (it != m_procMap.end()) {
    Proc* proc = it->second;
    line = proc->begLine();
    isfound = true;
  }
  DIAG_MsgIf(DBG_BLD_PROC_MAP, "LM::findProcSrcCodeInfo " 
	     << ival.toString() << " = " << line);

  return isfound;
}


void
BinUtil::LM::textBegEndVMA(VMA* begVMA, VMA* endVMA)
{ 
  VMA curr_begVMA;
  VMA curr_endVMA;
  VMA sml_begVMA = 0;
  VMA lg_endVMA  = 0;
  for (SegMap::iterator it = m_segMap.begin(); it != m_segMap.end(); ++it) {
    Seg* seg = it->second; 
    if (seg->type() == Seg::TypeText)  {    
      if (!(sml_begVMA || lg_endVMA)) {
	sml_begVMA = seg->begVMA();
	lg_endVMA  = seg->endVMA(); 
      } 
      else { 
	curr_begVMA = seg->begVMA();
	curr_endVMA = seg->endVMA(); 
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
BinUtil::LM::toString(int flags, const char* pre) const
{
  std::ostringstream os;
  dump(os, flags, pre);
  return os.str();
}


void
BinUtil::LM::dump(std::ostream& o, int flags, const char* pre) const
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
  dumpModuleInfo(o, p2.c_str());

  o << p1 << "Load Module Contents:\n";
  dumpme(o, p2.c_str());

  if (flags & DUMP_Flg_SymTab) {
    o << p2 << "Symbol Table (" << m_bfdSymTabSz << "):\n";
    dumpSymTab(o, p2.c_str());
  }
  
  o << p2 << "Sections (" << numSegs() << "):\n";
  for (SegMap::const_iterator it = segs().begin(); it != segs().end(); ++it) {
    Seg* seg = it->second;
    seg->dump(o, flags, p2.c_str());
  }
}


void
BinUtil::LM::ddump(int code) const
{
  dump(std::cerr, code);
}


void
BinUtil::LM::dumpme(std::ostream& GCC_ATTR_UNUSED o,
		    const char* GCC_ATTR_UNUSED pre) const
{
}



void
BinUtil::LM::dumpProcMap(std::ostream& os, unsigned flag, 
			  const char* GCC_ATTR_UNUSED pre) const
{
  for (ProcMap::const_iterator it = m_procMap.begin(); 
       it != m_procMap.end(); ++it) {
    os << it->first.toString() << " --> " << std::hex << "Ox" << it->second 
       << std::dec << endl;
    if (flag != 0) {
      os << it->second->toString();
    }
  }
}


void
BinUtil::LM::ddumpProcMap(unsigned flag) const
{
  dumpProcMap(std::cerr, flag);
}

//***************************************************************************

int
BinUtil::LM::cmpBFDSymByVMA(const void* s1, const void* s2)
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


void
BinUtil::LM::readSymbolTables()
{
  // -------------------------------------------------------
  // Read the normal symbol table
  // -------------------------------------------------------
  long bytesNeeded = bfd_get_symtab_upper_bound(m_bfd);
  
  if (bytesNeeded > 0) {
    m_bfdSymTab = new asymbol*[bytesNeeded / sizeof(asymbol*)];
    m_bfdSymTabSz = bfd_canonicalize_symtab(m_bfd, m_bfdSymTab);

    if (m_bfdSymTabSz == 0) {
      delete[] m_bfdSymTab;
      m_bfdSymTab = NULL;
      DIAG_Msg(2, "'" << name() << "': No regular symbols found.");
    }
  }

  // -------------------------------------------------------
  // Read the dynamic symbol table 
  // -------------------------------------------------------
  {
    bytesNeeded = bfd_get_dynamic_symtab_upper_bound(m_bfd);

    if (bytesNeeded > 0) {
      m_bfdDynSymTab = new asymbol*[bytesNeeded / sizeof(asymbol*)];
      m_bfdDynSymTabSz = bfd_canonicalize_dynamic_symtab(m_bfd, m_bfdDynSymTab);
    }

    if (m_bfdDynSymTabSz == 0) {
      DIAG_Msg(2, "'" << name() << "': No dynamic symbols found.");
    }
  }

  // -------------------------------------------------------
  // Append the synthetic symbol table to our copy for sorting.
  // On many platforms this is empty, but it helps on powerpc.
  //
  // Note: the synthetic table is an array of asymbol structs,
  // not an array of pointers, and not null-terminated.
  // Note: the sorted table may be larger than the original table,
  // and size is the size of the sorted table (regular + synthetic).
  // -------------------------------------------------------
  m_bfdSynthTabSz = bfd_get_synthetic_symtab(m_bfd, m_bfdSymTabSz, m_bfdSymTab, 
					     m_bfdDynSymTabSz, m_bfdDynSymTab, 
					     &m_bfdSynthTab);
  if (m_bfdSynthTabSz < 0) {
    m_bfdSynthTabSz = 0;
  }

  if (m_bfdSynthTabSz == 0) {
    DIAG_Msg(2, "'" << name() << "': No synthetic symbols found.");
  }

  m_bfdSymTabSort = new asymbol*[m_bfdSymTabSz + m_bfdSynthTabSz + 1];
  memcpy(m_bfdSymTabSort, m_bfdSymTab, m_bfdSymTabSz * sizeof(asymbol *));
  for (int i = 0; i < m_bfdSynthTabSz; i++) {
    m_bfdSymTabSort[m_bfdSymTabSz + i] = &m_bfdSynthTab[i];
  }
  m_bfdSymTabSortSz = m_bfdSymTabSz + m_bfdSynthTabSz;
  m_bfdSymTabSort[m_bfdSymTabSortSz] = NULL;

  // -------------------------------------------------------
  // Sort symbol table by VMA.
  // -------------------------------------------------------
  QuickSort QSort;
  QSort.Create((void **)(m_bfdSymTabSort), LM::cmpBFDSymByVMA);
  QSort.Sort(0, m_bfdSymTabSortSz - 1);
}


void
BinUtil::LM::readSegs()
{
  // Create sections.
  // Pass symbol table and debug summary information for each section
  // into that section as it is created.
  m_dbgInfo.read(m_bfd, m_bfdSymTab);

  // Process each section in the object file.
  for (asection* sec = m_bfd->sections; (sec); sec = sec->next) {

    // 1. Determine initial section attributes
    string segnm(bfd_section_name(m_bfd, sec));
    bfd_vma segBeg = bfd_section_vma(m_bfd, sec);
    uint64_t segSz = bfd_section_size(m_bfd, sec) / bfd_octets_per_byte(m_bfd);
    bfd_vma segEnd = segBeg + segSz;
    
    // 2. Create section
    Seg* seg = NULL;
    if (sec->flags & SEC_CODE) {
      seg = new TextSeg(this, segnm, segBeg, segEnd, segSz);
    }
    else {
      seg = new Seg(this, segnm, Seg::TypeData, segBeg, segEnd, segSz);
    }
    bool ins = insertSeg(VMAInterval(segBeg, segEnd), seg);
    if (!ins) {
      DIAG_WMsg(3, "Overlapping segment: " << segnm << ": " 
		<< std::hex << segBeg << " " << segEnd << std::dec);
      delete seg;
    }
  }

  m_dbgInfo.clear();
}

void
BinUtil::LM::computeNoReturns()
{
  m_noreturns = new NoReturns();
  
  // gather information about functions in this load module
  m_noreturns->addSymEntries(m_bfdSymTab, m_bfdSymTabSz);

  // gather information about functions accessed through the PLT
  m_noreturns->addSynSymEntries(m_bfdSynthTab, m_bfdSynthTabSz);
}


bool
BinUtil::LM::functionNeverReturns(VMA addr)
{
   return m_noreturns->isNoReturn(addr);
}


void
BinUtil::LM::dumpModuleInfo(std::ostream& o, const char* pre) const
{
  string p(pre);

  o << p << "Name: `" << name() << "'\n";

  o << p << "Format: `" << bfd_get_target(m_bfd) << "'" << endl;
  // bfd_get_flavour

  o << p << "Type: `";
  switch (type()) {
    case TypeNULL:
      o << "Unknown load module type'\n";
      break;
    case TypeExe:    
      o << "Executable (fully linked except for possible DSOs)'\n";
      break;
    case TypeDSO: 
      o << "Dynamically Shared Library'\n";
      break;
    default:
      DIAG_Die("Invalid load module type: " << type());
  }
  
  o << p << "Load VMA: " << std::hex << m_begVMA << std::dec << "\n";

  o << p << "Text(beg,end): " 
    << std::hex << textBeg() << ", " << textEnd() << std::dec << "\n";
  
  o << p << "Endianness: `"
    << ( (bfd_big_endian(m_bfd)) ? "Big'\n" : "Little'\n" );
  
  o << p << "Architecture: `";
  switch (bfd_get_arch(m_bfd)) {
    case bfd_arch_alpha:   o << "Alpha'\n"; break;
    case bfd_arch_mips:    o << "MIPS'\n";  break;
    case bfd_arch_powerpc: o << "POWER'\n"; break;
    case bfd_arch_sparc:   o << "SPARC'\n"; break;
    case bfd_arch_i386:    o << "x86'\n";   break;
    case bfd_arch_ia64:    o << "IA-64'\n"; break; 
#ifdef bfd_mach_k1om
    case bfd_arch_k1om:    o << "K1OM'\n";  break;
#endif
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
    case bfd_arch_powerpc:
      switch (bfd_get_mach(m_bfd)) {
        case bfd_mach_ppc:   o << "PPC'\n"; break;
        case bfd_mach_ppc64: o << "PPC-64'\n"; break;
        default:             o << "-unknown POWER-'\n";
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
#ifdef bfd_mach_k1om
    case bfd_arch_k1om:
      o << "K1OM'\n";
      break;
#endif
    default: 
      DIAG_Die("Unknown bfd arch: " << bfd_get_arch(m_bfd));
  }
  
  o << p << "Bits per byte: "    << bfd_arch_bits_per_byte(m_bfd)    << endl;
  o << p << "Bits per address: " << bfd_arch_bits_per_address(m_bfd) << endl;
  o << p << "Bits per word: "    << m_bfd->arch_info->bits_per_word  << endl;
}


static void
dumpASymbol(std::ostream& o, asymbol* sym, string p1)
{
  // value, name, section name
  o << p1 << std::hex << std::setw(16)
    << (bfd_vma)bfd_asymbol_value(sym) << ": " << std::setw(0) << std::dec
    << bfd_asymbol_name(sym)
    << " [sec: " << sym->section->name << "] ";

  // flags
  o << "[flg: " << std::hex << sym->flags << std::dec << " ";
  bool hasPrintedFlag = false;
  dumpSymFlag(o, sym, BSF_LOCAL,        "LCL",     hasPrintedFlag);
  dumpSymFlag(o, sym, BSF_GLOBAL,       "GBL",     hasPrintedFlag);
  dumpSymFlag(o, sym, BSF_FUNCTION,     "FUNC",    hasPrintedFlag);
  dumpSymFlag(o, sym, BSF_WEAK,         "WEAK",    hasPrintedFlag);
  dumpSymFlag(o, sym, BSF_SECTION_SYM,  "SEC",     hasPrintedFlag);
  dumpSymFlag(o, sym, BSF_FILE,         "FILE",    hasPrintedFlag);
  dumpSymFlag(o, sym, BSF_DYNAMIC,      "DYN",     hasPrintedFlag);
  dumpSymFlag(o, sym, BSF_OBJECT,       "OBJ",     hasPrintedFlag);
  dumpSymFlag(o, sym, BSF_THREAD_LOCAL, "THR_LCL", hasPrintedFlag);
  o << "]";

  if (BinUtil::Proc::isProcBFDSym(sym)) {
    o << " *proc*";
  }

  o << endl;
}


void
BinUtil::LM::dumpSymTab(std::ostream& o, const char* pre) const
{
  string p(pre);
  string p1 = p + "  ";

  o << p << "--------------- Symbol Table Dump (Unsorted) --------------\n";

  if (m_bfdSymTab) {
    for (uint i = 0; m_bfdSymTab[i] != NULL; i++) {
      dumpASymbol(o, m_bfdSymTab[i], p1);
    }
  }

  o << p << "--------------- Symbol Table Dump (Synthetic) -------------\n";

  if (m_bfdSynthTabSz) {
    for (int i = 0; i < m_bfdSynthTabSz; i++) {
      dumpASymbol(o, &m_bfdSynthTab[i], p1);
    }
  }

  o << p << "-----------------------------------------------------------\n";
}


static void
dumpSymFlag(std::ostream& o, 
	    asymbol* sym, int flag, const char* txt, bool& hasPrinted)
{
  if ((sym->flags & flag)) {
    if (hasPrinted) {
      o << ",";
    }
    o << txt;
    hasPrinted = true;			\
  }
}




//***************************************************************************
// Exe
//***************************************************************************

BinUtil::Exe::Exe()
  : m_startVMA(0)
{
}


BinUtil::Exe::~Exe()
{
}


void
BinUtil::Exe::open(const char* filenm)
{
  LM::open(filenm);
  if (type() != LM::TypeExe) {
    BINUTIL_Throw("'" << filenm << "' is not an executable.");
  }

  m_startVMA = bfd_get_start_address(abfd());
}


void
BinUtil::Exe::dump(std::ostream& o, int flags, const char* pre) const
{
  LM::dump(o, flags, pre);
}


void
BinUtil::Exe::dumpme(std::ostream& o, const char* pre) const
{
  o << pre << "Program start address: " << std::hex << getStartVMA()
    << std::dec << endl;
}

//***************************************************************************
