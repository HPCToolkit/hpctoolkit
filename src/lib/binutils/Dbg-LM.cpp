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

#include <iostream>
using std::cerr;
using std::endl;
using std::hex;
using std::dec;

#include <sstream>

//*************************** User Include Files ****************************

#include <include/hpctoolkit-config.h>
#include <include/gnu_bfd.h>

#include "Dbg-LM.hpp"
#include "Dbg-Proc.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/RealPathMgr.hpp>
#include <lib/support/SrcFile.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// LM
//***************************************************************************

BinUtil::Dbg::LM::LM()
{
}


BinUtil::Dbg::LM::~LM()
{
  clear();
  clear1();
}


void
BinUtil::Dbg::LM::read(bfd* abfd, asymbol** bfdSymTab)
{
#if defined(HAVE_HPC_GNUBINUTILS)
  if (!bfdSymTab) { return; }
  
  // Construct: Currently we only know about ELF/DWARF2
  if (bfd_get_flavour(abfd) == bfd_target_elf_flavour) {
    bfd_forall_dbg_funcinfo_fn_t callback = 
      (bfd_forall_dbg_funcinfo_fn_t)bfd_dbgInfoCallback;
    //int cnt =
    bfd_elf_forall_dbg_funcinfo(abfd, bfdSymTab, callback, this);
  }
  
  // Post-process and set parent pointers
  setParentPointers();
  //dump(std::cout);
#endif /* HAVE_HPC_GNUBINUTILS */
}


void
BinUtil::Dbg::LM::clear()
{
  for (const_iterator it = this->begin(); it != this->end(); ++it) {
    delete it->second;
  }
  m_map.clear();
}


void
BinUtil::Dbg::LM::clear1()
{
  for (const_iterator1 it = this->begin1(); it != this->end1(); ++it) {
    delete it->second;
  }
  m_map1.clear();
}


std::string
BinUtil::Dbg::LM::toString() const
{
  std::ostringstream os;
  dump(os);
  return os.str();
}


std::ostream&
BinUtil::Dbg::LM::dump(std::ostream& os) const
{
  os << "{ Dbg::LM: \n";
  for (const_iterator it = this->begin(); it != this->end(); ++it) {
    os << "(" << hex << it->first << dec << " --> " << it->second << ") ";
    it->second->dump(os);
  }
  for (const_iterator1 it = this->begin1(); it != this->end1(); ++it) {
    os << "(" << it->first << " --> " << it->second << ") ";
    it->second->dump(os);
  }
  os << "}\n";
  os.flush();
  return os;
}


void
BinUtil::Dbg::LM::ddump() const
{
  dump(std::cerr);
}


//***************************************************************************

// Should have function type of 'bfd_forall_dbg_funcinfo_fn_t'
int 
BinUtil::Dbg::LM::bfd_dbgInfoCallback(void* callback_obj,
				      void* parent, void* funcinfo)
{
#if defined(HAVE_HPC_GNUBINUTILS)
  Dbg::LM* lminfo = (Dbg::LM*)callback_obj;
  
  Dbg::Proc* pinfo = new Dbg::Proc;

  // Collect information for 'funcinfo'
  bfd_vma begVMA, endVMA;
  const char* name, *filenm;
  unsigned int begLine;
  bfd_forall_dbg_funcinfo_get_decl_info(funcinfo, &name, &filenm, &begLine);
  bfd_forall_dbg_funcinfo_get_vma_bounds(funcinfo, &begVMA, &endVMA);
  if (!name)   { name = ""; }
  if (!filenm) { filenm = ""; }
  
  pinfo->begVMA = begVMA;
  pinfo->endVMA = endVMA;
  pinfo->name = name;
  pinfo->filenm = filenm;
  pinfo->begLine = (SrcFile::ln)begLine;

  // normalize file names -- sometimes filenames are of different
  // forms within same load module
  RealPathMgr::singleton().realpath(pinfo->filenm);

  // We are not guaranteed to see parent routines before children.
  // Save begVMA for future processing.
  begVMA = endVMA = 0;
  if (parent) {
    bfd_forall_dbg_funcinfo_get_vma_bounds(parent, &begVMA, &endVMA);
  }
  pinfo->parentVMA = begVMA;

  DIAG_DevMsg(10, "BinUtil::Dbg::LM::bfd_dbgInfoCallback:\n"
	      << pinfo->toString());


  // -------------------------------------------------------
  // store this 'pinfo' if it is of use
  //
  // NOTE: in the presense of inlining we may get multiple callbacks
  // for what appears to be the same routine when only looking at
  // begin VMAs:
  //
  // <1><93510>: Abbrev Number: 32 (DW_TAG_subprogram)
  //  <93511>     DW_AT_external    : 1	
  //  <93512>     DW_AT_name        : moo
  //  <93519>     DW_AT_low_pc      : 0x421520	
  //
  // <2><935c9>: Abbrev Number: 36 (DW_TAG_inlined_subroutine)
  //  <935ca>     DW_AT_abstract_origin: <8b982> [moo]
  //  <935ce>     DW_AT_low_pc      : 0x421520	
  //
  // Since all sorts of havok is created if the second entry is
  // preferred to the first, there really should be a bit to
  // distinguish between DW_TAG_subprogram and
  // DW_TAG_inlined_subroutine.  Below I employ the heuristic of
  // keeping the entry with the widest VMA range.
  // -------------------------------------------------------

  if (pinfo->begVMA != 0) {
    std::pair<iterator, bool> fnd = 
      lminfo->insert(std::make_pair(pinfo->begVMA, pinfo));
    if (!fnd.second) {
      Dbg::Proc* map_pinfo = fnd.first->second;
      Dbg::Proc* tokeep = map_pinfo;
      Dbg::Proc* todel = pinfo;
      if (pinfo->endVMA > map_pinfo->endVMA) {
	tokeep = pinfo;
	todel = map_pinfo;
	(*lminfo)[pinfo->begVMA] = tokeep;
      }
      DIAG_Msg(5, "Multiple DWARF descriptors for the same procedure (VMA):\nkeeping: " << tokeep->toString() << "deleting: " << todel->toString());
      delete todel;
    }
  }
  else if (!pinfo->filenm.empty()) {
    std::pair<iterator1, bool> fnd = 
      lminfo->insert1(std::make_pair(pinfo->name, pinfo));
    if (!fnd.second) {
      Dbg::Proc* map_pinfo = fnd.first->second;
      Dbg::Proc* tokeep = map_pinfo;
      Dbg::Proc* todel = pinfo;
      if (pinfo->begVMA < map_pinfo->begVMA 
	  || pinfo->endVMA > map_pinfo->endVMA) {
	tokeep = pinfo;
	todel = map_pinfo;
	(*lminfo)[pinfo->name] = tokeep;
      }
      DIAG_Msg(5, "Multiple DWARF descriptors for the same procedure (name):\nkeeping: " << tokeep->toString() << "deleting: " << todel->toString());
      delete todel;
      // FIXME: Would it be a good idea to delete *all* entries if
      // line numbers and files names don't match?
    }
  }
  else {
    delete pinfo; // the 'pinfo' is useless
  }
#endif /* HAVE_HPC_GNUBINUTILS */
  
  return 0;
}


void 
BinUtil::Dbg::LM::setParentPointers()
{
  // Set parent pointers assuming begVMA has been set.
  for (const_iterator it = this->begin(); it != this->end(); ++it) {
    Dbg::Proc* x = it->second;
    if (x->parentVMA != 0) {
      Dbg::Proc* parent = (*this)[x->parentVMA];
      if (x != parent) {
	x->parent = parent; // sanity check
      }
    }
  }
}

//***************************************************************************
