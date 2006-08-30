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
//    $Source$
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

#include <set>
#include <map>
#include <iostream>
#include <sstream>

//*************************** User Include Files ****************************

#include <include/general.h>

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
    : mBeg(beg), mEnd(end) { }
  VMAInterval(const char* formattedstr)
    { fromString(formattedstr); }

  VMAInterval(const VMAInterval& x)
    { *this = x; }

  VMAInterval& operator=(const VMAInterval& x) { 
    mBeg = x.mBeg;
    mEnd = x.mEnd;
    return *this; 
  }

  ~VMAInterval() { }

  // -------------------------------------------------------
  // data
  // -------------------------------------------------------
  VMA  beg() const { return mBeg; }
  VMA& beg()       { return mBeg; }
  
  VMA  end() const { return mEnd; }
  VMA& end()       { return mEnd; }

  bool empty() const { return mBeg >= mEnd; }

  // -------------------------------------------------------
  // interval comparison
  // -------------------------------------------------------
  
  // overlaps: does this interval overlap x
  bool overlaps(VMAInterval x) const;
  
  // contains: does this interval contain x
  bool contains(VMAInterval x) const
    { return ((beg() <= x.beg()) && (end() >= x.end())); }
  
  // -------------------------------------------------------
  // print/read/write
  // -------------------------------------------------------

  // Format: "[lb1-ub1)"
  std::string toString() const;

  void fromString(const char* formattedstr);
  void fromString(std::string& formattedstr)
    { fromString(formattedstr.c_str()); }

  std::ostream& dump(std::ostream& os) const;
  std::istream& slurp(std::istream& is);
  
  void ddump() const;
  
private:
  VMA mBeg;
  VMA mEnd;
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
  bool operator() (const VMAInterval& x, const VMAInterval& y) const {
    return (x < y);
  }
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
  VMAIntervalSet() { }
  VMAIntervalSet(const char* formattedstr)
    { fromString(formattedstr); }
  ~VMAIntervalSet() { }

  // -------------------------------------------------------
  // cloning (proscribe by hiding copy constructor and operator=)
  // -------------------------------------------------------
  
  // -------------------------------------------------------
  // iterator, find/insert, etc
  // -------------------------------------------------------
  // use inherited std::set routines

  // insert: Insert a VMA or VMAInterval and maintain the
  // non-overlapping interval invariant
  std::pair<iterator, bool> insert(const VMA beg, const VMA end)
    { return insert(value_type(beg, end)); }
  std::pair<iterator, bool> insert(const value_type& x);

  // erase: Erase a VMA or VMAInterval and maintain the
  // non-overlapping interval invariant
  size_type erase(const VMA beg, const VMA end)
    { return erase(value_type(beg, end)); }
  size_type erase(const key_type& x);

  // find: []

  // merge: Merge 'x' with this set
  void merge(const VMAIntervalSet& x);

  // -------------------------------------------------------
  // print/read/write
  // -------------------------------------------------------

  // Format: space-separated list of intervals: "{[lb1-ub1) [lb2-ub2) ...}"
  std::string toString() const;

  void fromString(const char* formattedstr);
  void fromString(std::string& formattedstr)
    { fromString(formattedstr.c_str()); }

  std::ostream& dump(std::ostream& os) const;
  std::istream& slurp(std::istream& is);

  void ddump() const;


private:
  VMAIntervalSet(const VMAIntervalSet& x);
  VMAIntervalSet& operator=(const VMAIntervalSet& x) { return *this; }

private:
  
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

  ~VMAIntervalMap()
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
  //  find: Given a VMAInterval x, find the element mapped to the
  //    interval that equals or contains x.
  iterator find(const key_type& toFind)
  {
    // find [lb, ub) where lb is the first element !< toFind 
    reverse_iterator lb(this->lower_bound(toFind));
    
    // Reverse-search for match
    if (lb.base() == this->end() && !this->empty()) {
      lb = this->rbegin();
    }
    for ( ; lb != this->rend(); --lb) {
      const VMAInterval& vmaint = lb->first;
      if (vmaint.contains(toFind)) {
	return lb.base();
      }
      else if (vmaint < toFind) {
	break; // (toFind > vmaint) AND !vmaint.contains(toFind)
      }
    }
    
    return this->end();
  }
  
  const_iterator find(const key_type& x) const
    { return find(x); }
  
  // use inherited std::map routines
  
  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  virtual std::string toString() const
  {
    std::ostringstream os;
    dump(os);
    os << std::ends;
    return os.str();
  }
  
  virtual std::ostream& dump(std::ostream& os) const
  {
    for (const_iterator it = this->begin(); it != this->end(); ++it) {
      //it->first.dump(os);
      //os << " --> " << hex << "Ox" << it->second << dec << endl;
    }
  }
  
  std::ostream& ddump() const
  {
    dump(std::cerr);
  }

private:
  VMAIntervalMap(const VMAIntervalMap& x);
  VMAIntervalMap& operator=(const VMAIntervalMap& x) { return *this; }

private:
  
};


//***************************************************************************

#endif 
