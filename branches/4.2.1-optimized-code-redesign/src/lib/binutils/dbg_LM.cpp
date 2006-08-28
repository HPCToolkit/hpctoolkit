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

#include <typeinfo>

#include <string>
using std::string;

#include <iostream>
using std::cerr;
using std::endl;
using std::hex;
using std::dec;

//*************************** User Include Files ****************************

#include <include/gnu_bfd.h>

#include "dbg_LM.hpp"
#include "dbg_Proc.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// LM
//***************************************************************************

binutils::dbg::LM::LM()
{
}


binutils::dbg::LM::~LM()
{
  clear();
}


void
binutils::dbg::LM::read(bfd* abfd, asymbol** bfdSymTab)
{
  if (!bfdSymTab) { return; }
  
  // Construct: Currently we only know about ELF/DWARF2
  if (bfd_get_flavour(abfd) == bfd_target_elf_flavour) {
    bfd_forall_dbg_funcinfo_fn_t callback = 
      (bfd_forall_dbg_funcinfo_fn_t)bfd_dbgInfoCallback;
    int cnt = bfd_elf_forall_dbg_funcinfo(abfd, bfdSymTab, callback, this);
  }
  
  // Post-process and set parent pointers
  setParentPointers();
  //dump(std::cout);
}


void
binutils::dbg::LM::clear()
{
  for (const_iterator it = this->begin(); it != this->end(); ++it) {
    delete it->second;
  }
  mMap.clear();
}


std::ostream&
binutils::dbg::LM::dump(std::ostream& os) const
{
  os << "{ dbg::LM: \n";
  for (const_iterator it = this->begin(); it != this->end(); ++it) {
    //os << "(" << it->first << " --> " << it->second << ")\n";
    it->second->dump(os);
  }
  os << "}\n";
  os.flush();
  return os;
}


//***************************************************************************

// Should have function type of 'bfd_forall_dbg_funcinfo_fn_t'
int 
binutils::dbg::LM::bfd_dbgInfoCallback(void* callback_obj, 
				       void* parent, void* funcinfo)
{
  LM* lm = (LM*)callback_obj;
  
  dbg::Proc* pinfo = new dbg::Proc;

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
  pinfo->begLine = (suint)begLine;

  // We are not guaranteed to see parent routines before children.
  // Save begVMA for future processing.
  begVMA = endVMA = 0;
  if (parent) {
    bfd_forall_dbg_funcinfo_get_vma_bounds(parent, &begVMA, &endVMA);
  }
  pinfo->parentVMA = begVMA;

  // For now, we assume that if we don't know an address for
  // 'funcinfo', then we can't use it
  if (pinfo->begVMA == 0) {
    delete pinfo;
    return 0;
  }
  
  std::pair<iterator, bool> res = 
    lm->insert(std::make_pair(pinfo->begVMA, pinfo));
  if (!res.second) {
    // There are two descriptors for the same function.  I don't think
    // this should happen in practice...
    // pinfo->dump(std::cout);
    delete pinfo;    
  }

  return 0;
}


void 
binutils::dbg::LM::setParentPointers()
{
  // Set parent pointers assuming begVMA has been set.
  for (const_iterator it = this->begin(); it != this->end(); ++it) {
    dbg::Proc* x = it->second;
    if (x->parentVMA != 0) {
      x->parent = (*this)[x->parentVMA];
    }
  }
}

//***************************************************************************
