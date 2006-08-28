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

#ifndef binutils_dbg_LM_hpp 
#define binutils_dbg_LM_hpp

//************************* System Include Files ****************************

#include <string>
#include <deque>
#include <map>
#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "VMAInterval.hpp"

#include <lib/isa/ISATypes.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************
// LM (LoadModule)
//***************************************************************************

namespace binutils {

namespace dbg {

class Proc;

// --------------------------------------------------------------------------
// 'LM' represents debug information for a load module
//
// Note: Most references to VMA (virtual memory address) could be
// replaced with 'PC' (program counter) or IP (instruction pointer).
// --------------------------------------------------------------------------

class LM {
public:
  typedef VMA                                               key_type;
  typedef Proc*                                             mapped_type;
  
  typedef std::map<key_type, mapped_type>                   My_t;
  typedef std::pair<const key_type, mapped_type>            value_type;
  typedef My_t::key_compare                                 key_compare;
  typedef My_t::allocator_type                              allocator_type;
  typedef My_t::reference                                   reference;
  typedef My_t::const_reference                             const_reference;
  typedef My_t::iterator                                    iterator;
  typedef My_t::const_iterator                              const_iterator;
  typedef My_t::size_type                                   size_type;
  
public:
  LM();
  ~LM();
  
  void read(bfd* abfd, asymbol** bfdSymTab);
  
  // -------------------------------------------------------
  // iterator, find/insert, etc 
  // -------------------------------------------------------
  
  // iterators:
  iterator begin() 
    { return mMap.begin(); }
  const_iterator begin() const 
    { return mMap.begin(); }
  iterator end() 
    { return mMap.end(); }
  const_iterator end() const 
    { return mMap.end(); }
    
  // capacity:
  size_type size() const
    { return mMap.size(); }
    
  // element access:
  mapped_type& operator[](const key_type& x)
    { return mMap[x]; }
    
  // modifiers:
  std::pair<iterator, bool> insert(const value_type& x)
    { return mMap.insert(x); }
  iterator insert(iterator position, const value_type& x)
    { return mMap.insert(position, x); }
    
  void erase(iterator position) 
    { mMap.erase(position); }
  size_type erase(const key_type& x) 
    { return mMap.erase(x); }
  void erase(iterator first, iterator last) 
    { return mMap.erase(first, last); }
    
  void clear();
  
  // mMap operations:
  iterator find(const key_type& x)
    { return mMap.find(x); }
  const_iterator find(const key_type& x) const
    { return mMap.find(x); }
  size_type count(const key_type& x) const
    { return mMap.count(x); }
    
  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  std::ostream& dump(std::ostream& os) const;

private:

  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------

  // Callback for bfd_elf_forall_dbg_funcinfo
  static int bfd_dbgInfoCallback(void* callback_obj, void* parent, 
				 void* funcinfo);
  
  void setParentPointers();
  
private:
  My_t mMap;
};
  

} // namespace dbg

} // namespace binutils

//***************************************************************************

#endif // binutils_dbg_LM_hpp
