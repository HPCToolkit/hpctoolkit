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

//************************* System Include Files ****************************

#include <typeinfo>

//*************************** User Include Files ****************************

#include "VMAInterval.hpp"

//*************************** Forward Declarations **************************

using std::cerr;
using std::endl;
using std::hex;
using std::dec;

//***************************************************************************

//***************************************************************************
// VMAInterval
//***************************************************************************

String
VMAInterval::toString() const
{
  String self = "[0x"
    + String(mBeg, true /*hex*/) + "-0x" 
    + String(mEnd, true /*hex*/) + "]";
  return self;
}


void
VMAInterval::fromString(const char* formattedstr)
{
  
}


std::ostream&
VMAInterval::dump(std::ostream& os) const
{
  os << std::hex << "[0x" << mBeg << "-0x" << mEnd << "]" << std::dec;
  return os;
}


void
VMAInterval::ddump() const
{
  dump(std::cout);
}


//***************************************************************************
// VMAIntervalSet
//***************************************************************************

// Given an interval x to insert or delete from the interval set, the
// following cases are possible where x is represented by <> and
// existing intervals by {}.  
//
// We first find lb and ub such that: (lb <= x < ub) and lb != x.
//
//   1) x is mutually exclusive:
//      a. <>
//      b. <> {ub} ...
//      c. ... {lb} <>
//      d. ... {lb} <> {ub} ...
//   2) x overlaps only the lb interval
//      a.  ... {lb < } ... {} >
//      b.  ... {lb < } ... {} > {ub} ...
//   3) x overlaps only the ub interval:
//      a.  < {} ... { > ub} ...
//      b.  ... {lb} < {} ... { > ub} ...
//   4) x overlaps both lb and ub interval
//        ... {lb < } ... { > ub} ...
//   5) x subsumes all intervals, including
//        < {} ... {} >


// insert: See above for cases
std::pair<VMAIntervalSet::iterator, bool>
VMAIntervalSet::insert(const VMAIntervalSet::value_type& x)
{
  // find [lb, ub).  IOW, (lb = x < ub) and lb equiv x, since
  // intervals cannot be duplicated.
  std::pair<VMAIntervalSet::iterator, VMAIntervalSet::iterator> lu = 
    equal_range(x);
  VMAIntervalSet::iterator lb = lu.first;
  VMAIntervalSet::iterator ub = lu.second;

  // find (lb <= x < ub) such that lb != x
  if (lb != end()) {
    --lb;
  }
  
  // Detect the appropriate case
  if (lb == end() && ub == end()) { // size >= 1
    if (size() == 0) {
      // Case 1a: insert x
      My_t::insert(x);
    }
    else {
      // Case 5: erase everything and insert x
      My_t::clear();
      My_t::insert(x);
    }
  }
  else if (lb == end()) {
    if (x.end() < ub->beg()) {
      // Case 1b: insert x
      My_t::insert(x);
    }
    else {
      // Case 3a: erase [begin(), ub + 1); insert [x.beg(), ub->end()]
      VMAIntervalSet::iterator end = ub;
      My_t::erase(begin(), ++end);
      My_t::insert( VMAInterval(x.beg(), ub->end()) );
    }
  }
  else if (ub == end()) {
    if (lb->end() < x.beg()) {
      // Case 1c: insert x
      My_t::insert(x);
    }
    else {
      // Case 2a: erase [lb, end()); insert [lb->beg(), x.end()]
      My_t::erase(lb, end());
      My_t::insert( VMAInterval(lb->beg(), x.end()) );
    }
  }
  else {
    if (lb->end() < x.beg() && x.end() < ub->beg()) {
      // Case 1d: insert x
      My_t::insert(x);
    }
    else if (x.beg() < lb->end() && x.end() < ub->beg()) {
      // Case 2b: erase [lb, ub); insert [lb->beg(), x.end()]
      My_t::erase(lb, ub);
      My_t::insert( VMAInterval(lb->beg(), x.end()) );
    }
    else if (lb->end() < x.beg() && ub->beg() < x.end()) {
      // Case 3b: erase [lb + 1, ub + 1): insert [x.beg(), ub->end()]
      VMAIntervalSet::iterator beg = lb;
      VMAIntervalSet::iterator end = ub;
      My_t::erase(++beg, ++end);
      My_t::insert( VMAInterval(x.beg(), ub->end()) );
    }
    else {
      // Case 4: erase [lb, ub + 1); insert [lb->beg(), ub->end()].
      VMAIntervalSet::iterator end = ub;
      My_t::erase(lb, ++end);
      My_t::insert( VMAInterval(lb->beg(), ub->end()) );
    }
  }
}


// erase: See above for cases
VMAIntervalSet::size_type
VMAIntervalSet::erase(const VMAIntervalSet::key_type& x)
{
  // find [lb, ub).  IOW, (lb = x < ub) and lb equiv x, since
  // intervals cannot be duplicated.
  std::pair<VMAIntervalSet::iterator, VMAIntervalSet::iterator> lu = 
    equal_range(x);
  VMAIntervalSet::iterator lb = lu.first;
  VMAIntervalSet::iterator ub = lu.second;

  // find (lb <= x < ub) such that lb != x
  if (lb != end()) {
    --lb;
  }
  
  // Detect the appropriate case.  Note that we do not have to
  // explicitly consider Case 1 since it amounts to a NOP.
  if (lb == end() && ub == end()) { // size >= 1
    if (size() != 0) {
      // Case 5: erase everything
      My_t::clear();
    }
  }
  else if (lb == end()) {
    if ( !(x.end() < ub->beg()) ) {
      // Case 3a: erase [begin(), ub + 1); insert [x.end() + 1, ub->end()]
      VMAIntervalSet::iterator end = ub;
      My_t::erase(begin(), ++end);
      if (x.end() < ub->end()) {
	My_t::insert( VMAInterval(x.end() + 1, ub->end()) );
      }
    }
  }
  else if (ub == end()) {
    if ( !(lb->end() < x.beg()) ) {
      // Case 2a: erase [lb, end()); insert [lb->beg(), x.beg() - 1]
      My_t::erase(lb, end());
      if (lb->beg() < x.beg()) {
	My_t::insert( VMAInterval(lb->beg(), x.beg() - 1) );
      }
    }
  }
  else {
    if (x.beg() < lb->end() && x.end() < ub->beg()) {
      // Case 2b: erase [lb, ub); insert [lb->beg(), x.beg() - 1]
      My_t::erase(lb, ub);
      if (lb->beg() < x.beg()) {
	My_t::insert( VMAInterval(lb->beg(), x.beg() - 1) );
      }
    }
    else if (lb->end() < x.beg() && ub->beg() < x.end()) {
      // Case 3b: erase [lb + 1, ub + 1): insert [x.end() + 1, ub->end()]
      VMAIntervalSet::iterator beg = lb;
      VMAIntervalSet::iterator end = ub;
      My_t::erase(++lb, ++ub);
      if (x.end() < ub->end()) {
	My_t::insert( VMAInterval(x.end() + 1, ub->end()) );
      }
    }
    else if ( !(lb->end() < x.beg() && x.end() < ub->beg()) ) {
      // Case 4: erase [lb, ub + 1); 
      //   insert [lb->beg(), x.beg() - 1] and [x.end() + 1, ub->end()].
      VMAIntervalSet::iterator end = ub;
      My_t::erase(lb, ++end);
      if (lb->beg() < x.beg()) {
	My_t::insert( VMAInterval(lb->beg(), x.beg() - 1) );
      }
      if (x.end() < ub->end()) {
	My_t::insert( VMAInterval(x.end() + 1, ub->end()) );
      }
    }
  }
}


String
VMAIntervalSet::toString() const
{
  String self;
  for (const_iterator it = this->begin(); it != this->end(); ++it) {
    self += (*it).toString() + " ";
  }
  return self;
}


void
VMAIntervalSet::fromString(const char* formattedstr)
{
  
}


std::ostream&
VMAIntervalSet::dump(std::ostream& os) const
{
  for (const_iterator it = this->begin(); it != this->end(); ++it) {
    (*it).dump(os);
    os << " ";
  }
  os.flush();
  return os;
}


void
VMAIntervalSet::ddump() const
{
  dump(std::cout);
}


//***************************************************************************
