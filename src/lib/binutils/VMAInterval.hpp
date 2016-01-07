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
//    $HeadURL$
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef VMAInterval_hpp 
#define VMAInterval_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <sstream>

#include <set>
#include <map>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>

#include <lib/isa/ISATypes.hpp>

//*************************** Forward Declarations **************************


//***************************************************************************
// VMAInterval
//***************************************************************************

// --------------------------------------------------------------------------
// VMAInterval: Represents the VMA interval [beg, end)
// --------------------------------------------------------------------------
class VMAInterval
{
public:
  // -------------------------------------------------------
  // constructor/destructor
  // -------------------------------------------------------
  VMAInterval(VMA beg, VMA end)
    : m_beg(beg), m_end(end)
  { }

  VMAInterval(const char* formattedstr)
  { fromString(formattedstr); }

  VMAInterval(const VMAInterval& x)
    : m_beg(x.m_beg), m_end(x.m_end)
  { }

  VMAInterval&
  operator=(const VMAInterval& x)
  {
    if (this != &x) {
      m_beg = x.m_beg;
      m_end = x.m_end;
    }
    return *this;
  }

  ~VMAInterval()
  { }

  // -------------------------------------------------------
  // data
  // -------------------------------------------------------
  VMA
  beg() const
  { return m_beg; }
  
  void
  beg(VMA x)
  { m_beg = x; }
  
  VMA
  end() const
  { return m_end; }
  
  void
  end(VMA x)
  { m_end = x; }

  bool
  empty() const
  { return m_beg >= m_end; }

  static bool
  empty(VMA beg, VMA end)
  { return beg >= end; }

  // -------------------------------------------------------
  // interval comparison
  // -------------------------------------------------------
  
  // overlaps: does this interval overlap x:
  //       {interval}             
  //  a.     <x> 
  //  b.           <x>
  //  c.  <x>
  bool
  overlaps(VMAInterval x) const
  {
    return (contains(x) 
	    || (x.beg() <= beg() && beg() <  x.end())
	    || (x.beg() <  end() && end() <= x.end()));
  }
  
  // contains: does this interval contain x
  bool
  contains(VMAInterval x) const
  { return ((beg() <= x.beg()) && (end() >= x.end())); }
  
  // -------------------------------------------------------
  // print/read/write
  // -------------------------------------------------------

  // Format: "[lb1-ub1)"
  std::string
  toString() const;

  void
  fromString(const char* formattedstr);

  void
  fromString(std::string& formattedstr)
  { fromString(formattedstr.c_str()); }

  std::ostream&
  dump(std::ostream& os) const;
  
  std::istream&
  slurp(std::istream& is);
  
  void
  ddump() const;
  
private:
  VMA m_beg;
  VMA m_end;
};


// --------------------------------------------------------------------------
// operator <, lt_VMAInterval: for ordering VMAInterval: Should work
// for any kind of interval: [ ], ( ), [ ), ( ].  For example:
//   
//   [1,1]  < [1,2] --> true
//   [1,1]  < [1,1] --> false
//   [1,10] < [4,6] --> true
// --------------------------------------------------------------------------

inline bool 
operator<(const VMAInterval& x, const VMAInterval& y) 
{
  return ((x.beg() < y.beg()) || 
	  ((x.beg() == y.beg()) && (x.end() < y.end())));
}

class lt_VMAInterval {
public:
  bool
  operator() (const VMAInterval& x, const VMAInterval& y) const
  { return (x < y); }
};


// --------------------------------------------------------------------------
// operators
// --------------------------------------------------------------------------

inline bool 
operator==(const VMAInterval& x, const VMAInterval& y) 
{
  return ((x.beg() == y.beg()) && (x.end() == y.end()));
}

inline bool 
operator!=(const VMAInterval& x, const VMAInterval& y) 
{
  return ( !(x == y) );
}


//***************************************************************************
// VMAIntervalSet
//***************************************************************************

// --------------------------------------------------------------------------
// VMAIntervalSet: A set of *non-overlapping* VMAIntervals
// --------------------------------------------------------------------------
class VMAIntervalSet
  : public std::set<VMAInterval>
{
public:
  typedef VMAInterval                                       key_type;
  
  typedef std::set<key_type>                                My_t;
  typedef key_type                                          value_type;
  typedef My_t::key_compare                                 key_compare;
  typedef My_t::allocator_type                              allocator_type;
  typedef My_t::reference                                   reference;
  typedef My_t::const_reference                             const_reference;
  typedef My_t::iterator                                    iterator;
  typedef My_t::const_iterator                              const_iterator;
  typedef My_t::size_type                                   size_type;

public:
  // -------------------------------------------------------
  // constructor/destructor
  // -------------------------------------------------------
  VMAIntervalSet()
  { }

  VMAIntervalSet(const char* formattedstr)
  { fromString(formattedstr); }
  
  virtual ~VMAIntervalSet()
  { }

  // -------------------------------------------------------
  // cloning (proscribe by hiding copy constructor and operator=)
  // -------------------------------------------------------
  
  // -------------------------------------------------------
  // iterator, find/insert, etc
  // -------------------------------------------------------
  // use inherited std::set routines

  // insert: Insert a VMA or VMAInterval and maintain the
  // non-overlapping interval invariant. Merges intervals that overlap
  // or intersect only at a boundary.
  std::pair<iterator, bool>
  insert(const VMA beg, const VMA end)
  { return insert(value_type(beg, end)); }

  std::pair<iterator, bool>
  insert(const value_type& x);

  // erase: Erase a VMA or VMAInterval and maintain the
  // non-overlapping interval invariant
  void
  erase(const VMA beg, const VMA end)
  { return erase(value_type(beg, end)); }

  void
  erase(const key_type& x);

  // find: []

  // merge: Merge 'x' with this set
  void
  merge(const VMAIntervalSet& x);

  // -------------------------------------------------------
  // print/read/write
  // -------------------------------------------------------

  // Format: space-separated list of intervals: "{[lb1-ub1) [lb2-ub2) ...}"
  std::string
  toString() const;

  void
  fromString(const char* formattedstr);

  void
  fromString(std::string& formattedstr)
  { fromString(formattedstr.c_str()); }

  std::ostream&
  dump(std::ostream& os) const;

  std::istream&
  slurp(std::istream& is);

  void
  ddump() const;


private:
  VMAIntervalSet(const VMAIntervalSet& x);

  VMAIntervalSet&
  operator=(const VMAIntervalSet& GCC_ATTR_UNUSED x)
  { return *this; }

private:
  
};


// --------------------------------------------------------------------------
// operator <, lt_VMAIntervalSet: for ordering VMAIntervalSets based
// on first entry.  For example:
//   
//   {}       < {}      --> false
//   {}       < {[1,2]} --> true
//   {[1,1]}  < {}      --> false
//   {[1,1]}  < {[1,2]} --> true
//   {[1,1]}  < {[1,1]} --> false
//   {[1,10]} < {[4,6]} --> true
// --------------------------------------------------------------------------

inline bool 
operator<(const VMAIntervalSet& x, const VMAIntervalSet& y)
{
  if (x.size() == 0 && y.size() >= 1) {  // x < y
    return true;
  }
  else if (x.size() >= 1 && y.size() >= 1) {
    const VMAInterval& xfirst = *(x.begin());
    const VMAInterval& yfirst = *(y.begin());
    return (xfirst < yfirst);
  }
  else {
    // (x.size() == 0 && y.size() == 0) || (x.size() >= 1 && y.size() == 0)
    return false;
  }
}

class lt_VMAIntervalSet {
public:
  bool operator() (const VMAIntervalSet& x, const VMAIntervalSet& y) const {
    return (x < y);
  }
};



//***************************************************************************
// VMAIntervalMap
//***************************************************************************

// --------------------------------------------------------------------------
// VMAIntervalMap: maps *non-overlapping* intervals to some type T.
// Provides an interface for finding entries based contained intervals.
// --------------------------------------------------------------------------

template <typename T>
class VMAIntervalMap
  : public std::map<VMAInterval, T>
{
public:
  typedef VMAInterval                                       key_type;
  typedef T                                                 mapped_type;
  
  typedef std::map<key_type, T>                             My_t;
  typedef std::pair<const key_type, T>                      value_type;
  typedef typename My_t::key_compare                        key_compare;
  typedef typename My_t::allocator_type                     allocator_type;
  typedef typename My_t::reference                          reference;
  typedef typename My_t::const_reference                    const_reference;
  typedef typename My_t::iterator                           iterator;
  typedef typename My_t::const_iterator                     const_iterator;
  typedef typename My_t::reverse_iterator                   reverse_iterator;
  typedef typename My_t::const_reverse_iterator             const_reverse_iterator;
  typedef typename My_t::size_type                          size_type;

public:
  // -------------------------------------------------------
  // constructor/destructor
  // -------------------------------------------------------
  VMAIntervalMap()
  { }

  virtual ~VMAIntervalMap()
  { }

  // -------------------------------------------------------
  // cloning (proscribe by hiding copy constructor and operator=)
  // -------------------------------------------------------
  
  // -------------------------------------------------------
  // iterator, find/insert, etc
  // -------------------------------------------------------
  
  // element access:
  //mapped_type& operator[](const key_type& x);

  // mMap operations:

  // find: Given a VMAInterval x, find the element mapped to the
  //   interval that equals or contains x.
  iterator
  find(const key_type& toFind)
  {
    // find lb where lb is the first element !< x;
    reverse_iterator lb_r(this->lower_bound(toFind));
    if (lb_r.base() == this->end() && !this->empty()) {
      lb_r = this->rbegin();
    }
    else if (!this->empty()) {
      lb_r--; // adjust for reverse iterators
    }
    
    // Reverse-search for match
    for ( ; lb_r != this->rend(); ++lb_r) {
      const VMAInterval& vmaint = lb_r->first;
      if (vmaint.contains(toFind)) {
	return --(lb_r.base()); // adjust for reverse iterators
      }
      else if (vmaint < toFind) {
      	break; // !vmaint.contains(toFind) AND (toFind > vmaint)
      }
    }
    
    return this->end();
  }
  
  const_iterator
  find(const key_type& x) const
  { return const_cast<VMAIntervalMap*>(this)->find(x); }

  
  // use inherited std::map routines
  
  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  std::string
  toString() const
  {
    std::ostringstream os;
    dump(os);
    return os.str();
  }
  
  std::ostream&
  dump(std::ostream& os) const
  {
    for (const_iterator it = this->begin(); it != this->end(); ++it) {
      os << it->first.toString() << " --> " << it->second << std::endl;
    }
    return os;
  }
  
  std::ostream&
  ddump() const
  {
    return dump(std::cerr);
  }

private:
  VMAIntervalMap(const VMAIntervalMap& x);

  VMAIntervalMap&
  operator=(const VMAIntervalMap& x)
  { return *this; }

private:
  
};


//***************************************************************************

#endif 
