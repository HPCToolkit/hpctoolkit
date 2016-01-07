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

#ifndef BinUtil_Dbg_LM_hpp 
#define BinUtil_Dbg_LM_hpp

//************************* System Include Files ****************************

#include <string>
#include <deque>
#include <map>
#include <iostream>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "VMAInterval.hpp"

#include <lib/isa/ISATypes.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************
// LM (LoadModule)
//***************************************************************************

namespace BinUtil {

namespace Dbg {

class Proc;

// --------------------------------------------------------------------------
// 'LM' represents debug information for a load module
//
// Note: Most references to VMA (virtual memory address) could be
// replaced with 'PC' (program counter) or IP (instruction pointer).
// --------------------------------------------------------------------------

class LM {
public:
  // A VMA -> Dbg::Proc map (could use VMAInterval -> Dbg::Proc)
  typedef VMA                                               key_type;
  typedef Dbg::Proc*                                        mapped_type;
  
  typedef std::map<key_type, mapped_type>                   My_t;
  typedef std::pair<const key_type, mapped_type>            value_type;
  typedef My_t::key_compare                                 key_compare;
  typedef My_t::allocator_type                              allocator_type;
  typedef My_t::reference                                   reference;
  typedef My_t::const_reference                             const_reference;
  typedef My_t::iterator                                    iterator;
  typedef My_t::const_iterator                              const_iterator;
  typedef My_t::size_type                                   size_type;

  // A string -> Dbg::Proc map (could use VMAInterval -> Dbg::Proc)
  typedef std::string                                       key_type1;

  typedef std::map<key_type1, mapped_type>                  My1_t;
  typedef std::pair<const key_type1, mapped_type>           value_type1;
  typedef My1_t::key_compare                                key_compare1;
  typedef My1_t::allocator_type                             allocator_type1;
  typedef My1_t::reference                                  reference1;
  typedef My1_t::const_reference                            const_reference1;
  typedef My1_t::iterator                                   iterator1;
  typedef My1_t::const_iterator                             const_iterator1;
  typedef My1_t::size_type                                  size_type1;

public:
  LM();
  ~LM();
  
  void read(bfd* abfd, asymbol** bfdSymTab);
  
  // -------------------------------------------------------
  // iterator, find/insert, etc [My_t]
  // -------------------------------------------------------
  
  // iterators:
  iterator begin() 
    { return m_map.begin(); }
  const_iterator begin() const 
    { return m_map.begin(); }
  iterator end() 
    { return m_map.end(); }
  const_iterator end() const 
    { return m_map.end(); }
    
  // capacity:
  size_type size() const
    { return m_map.size(); }
    
  // element access:
  mapped_type& operator[](const key_type& x)
    { return m_map[x]; }
    
  // modifiers:
  std::pair<iterator, bool> insert(const value_type& x)
    { return m_map.insert(x); }
  iterator insert(iterator position, const value_type& x)
    { return m_map.insert(position, x); }
    
  void erase(iterator position) 
    { m_map.erase(position); }
  size_type erase(const key_type& x) 
    { return m_map.erase(x); }
  void erase(iterator first, iterator last) 
    { m_map.erase(first, last); }
    
  void clear();
  
  // m_map operations:
  iterator find(const key_type& x)
    { return m_map.find(x); }
  const_iterator find(const key_type& x) const
    { return m_map.find(x); }
  size_type count(const key_type& x) const
    { return m_map.count(x); }


  // -------------------------------------------------------
  // iterator, find/insert, etc [My1_t]
  // -------------------------------------------------------
  
  // iterators:
  iterator1 begin1() 
    { return m_map1.begin(); }
  const_iterator1 begin1() const 
    { return m_map1.begin(); }
  iterator1 end1() 
    { return m_map1.end(); }
  const_iterator1 end1() const 
    { return m_map1.end(); }
    
  // capacity:
  size_type size1() const
    { return m_map1.size(); }
    
  // element access:
  mapped_type& operator[](const key_type1& x)
    { return m_map1[x]; }
    
  // modifiers:
  std::pair<iterator1, bool> insert1(const value_type1& x)
    { return m_map1.insert(x); }
  iterator1 insert1(iterator1 position, const value_type1& x)
    { return m_map1.insert(position, x); }
    
  void erase1(iterator1 position) 
    { m_map1.erase(position); }
  size_type erase1(const key_type1& x) 
    { return m_map1.erase(x); }
  void erase1(iterator1 first, iterator1 last) 
    { m_map1.erase(first, last); }
    
  void clear1();
  
  // m_map operations:
  iterator1 find1(const key_type1& x)
    { return m_map1.find(x); }
  const_iterator1 find1(const key_type1& x) const
    { return m_map1.find(x); }
  size_type count1(const key_type1& x) const
    { return m_map1.count(x); }

    
  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  std::string toString() const;

  std::ostream& dump(std::ostream& os) const;

  void ddump() const;

private:

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  // Callback for bfd_elf_forall_dbg_funcinfo
  static int bfd_dbgInfoCallback(void* callback_obj, void* parent, 
				 void* funcinfo);
  
  void setParentPointers();
  
private:
  My_t m_map;
  My1_t m_map1;
};
  

} // namespace Dbg

} // namespace BinUtil

//***************************************************************************

#endif // BinUtil_Dbg_LM_hpp
