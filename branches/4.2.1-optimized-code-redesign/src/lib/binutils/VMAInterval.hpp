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

//*************************** User Include Files ****************************

#include <include/general.h>

#include <lib/isa/ISATypes.hpp>

#include <lib/support/String.hpp>

//*************************** Forward Declarations **************************


//***************************************************************************
// VMAInterval
//***************************************************************************

// --------------------------------------------------------------------------
// VMAInterval: Represents an inclusive VMA interval [beg, end]
// --------------------------------------------------------------------------
class VMAInterval
{
public:
  VMAInterval(VMA beg, VMA end)
    : mBeg(beg), mEnd(end) { }
  VMAInterval(const char* formattedstr)
    { fromString(formattedstr); }

  ~VMAInterval() { }
  
  VMA  beg() const { return mBeg; }
  VMA& beg()       { return mBeg; };
  
  VMA  end() const { return mEnd; }
  VMA& end()       { return mEnd; }

  // -------------------------------------------------------
  // printing/slurping
  // -------------------------------------------------------

  // Format: "[lb1-ub1]"
  String toString() const;
  void   fromString(const char* formattedstr);

  std::ostream& dump(std::ostream& os) const;
  std::istream& slurp(std::istream& is);
  
  void ddump() const;
  
private:
  VMA mBeg;
  VMA mEnd;
};


// --------------------------------------------------------------------------
// lt_VMAInterval: for ordering VMAInterval:
//   [1,1]  < [1,2] --> true
//   [1,1]  < [1,1] --> false
//   [1,10] < [4,6] --> true
// --------------------------------------------------------------------------
class lt_VMAInterval {
public:
  bool operator() (const VMAInterval& x, const VMAInterval& y) const {
    return ((x.beg() < y.beg()) || 
	    ((x.beg() == y.beg()) && (x.end() < y.end())));
  }
};


//***************************************************************************
// VMAIntervalSet
//***************************************************************************

// --------------------------------------------------------------------------
// VMAIntervalSet: A set of non-overlapping VMAIntervals (inclusive
// intervals)
// --------------------------------------------------------------------------
class VMAIntervalSet
  : public std::set<VMAInterval, lt_VMAInterval>
{
public:
  typedef VMAInterval                                       key_type;
  typedef lt_VMAInterval                                    cmp_type;
  
  typedef std::set<key_type, cmp_type>                      My_t;
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

  // Insert a VMA or VMAInterval and maintain the non-overlapping
  // interval invariant
  std::pair<iterator, bool> insert(const VMA x)
    { return insert(value_type(x,x)); }
  std::pair<iterator, bool> insert(const value_type& x);

  // Erase a VMA or VMAInterval and maintain the non-overlapping
  // interval invariant
  size_type erase(const VMA x)
    { return erase(value_type(x,x)); }
  size_type erase(const key_type& x);

  // -------------------------------------------------------
  // printing/slurping
  // -------------------------------------------------------

  // Format: space-separated list of intervals: "[lb1-ub1] [lb2-ub2] ..."
  String toString() const;
  void   fromString(const char* formattedstr);
  
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
// VMAIntervalMap: 
// --------------------------------------------------------------------------

template <typename T>
class VMAIntervalMap
  : public std::map<VMAInterval, T, lt_VMAInterval>
{
public:
  // -------------------------------------------------------
  // constructor/destructor
  // -------------------------------------------------------
  VMAIntervalMap();
  ~VMAIntervalMap();

  // -------------------------------------------------------
  // cloning (proscribe by hiding copy constructor and operator=)
  // -------------------------------------------------------
  
  // -------------------------------------------------------
  // iterator, find/insert, etc
  // -------------------------------------------------------
  // use inherited std::map routines
  
  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  std::ostream& ddump() const;
  std::ostream& dump(std::ostream& os) const;

private:
  VMAIntervalMap(const VMAIntervalMap& x);
  VMAIntervalMap& operator=(const VMAIntervalMap& x) { return *this; }

private:
  
};


//***************************************************************************

#endif 
