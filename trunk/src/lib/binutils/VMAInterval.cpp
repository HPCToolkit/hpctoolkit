// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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

#include <iostream>
using std::cerr;
using std::endl;
using std::hex;
using std::dec;

#include <typeinfo>

#include <string>
using std::string;

//*************************** User Include Files ****************************

#include "VMAInterval.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/StrUtil.hpp>

//*************************** Forward Declarations **************************

#define DBG 0

//***************************************************************************

//***************************************************************************
// VMAInterval
//***************************************************************************

string
VMAInterval::toString() const
{
  string self = "["
    + StrUtil::toStr(mBeg, 16) + "-" 
    + StrUtil::toStr(mEnd, 16) + ")";
  return self;
}


void
VMAInterval::fromString(const char* formattedstr)
{
  const char* s = formattedstr;
  const char* p = formattedstr;
  if (!p) { return; }
  
  // -----------------------------------------------------
  // 1. ignore leading whitespace
  // -----------------------------------------------------
  while (*p != '\0' && isspace(*p)) { p++; }
  if (*p == '\0') { return; }

  // -----------------------------------------------------
  // 2. parse
  // -----------------------------------------------------
  // skip '['
  DIAG_Assert(*p == '[', DIAG_UnexpectedInput << "'" << s << "'");
  p++;

  // read 'mBeg'
  unsigned endidx;
  mBeg = StrUtil::toUInt64(p, &endidx);
  p += endidx;
 
  // skip '-'
  DIAG_Assert(*p == '-', DIAG_UnexpectedInput << "'" << s << "'");
  p++;
  
  // read 'mEnd'
  mEnd = StrUtil::toUInt64(p, &endidx);
  p += endidx;
  
  // skip ')'
  DIAG_Assert(*p == ')', DIAG_UnexpectedInput << "'" << s << "'");
}


std::ostream&
VMAInterval::dump(std::ostream& os) const
{
  os << std::hex << "[0x" << mBeg << "-0x" << mEnd << ")" << std::dec;
  return os;
}


void
VMAInterval::ddump() const
{
  dump(std::cout);
  std::cout.flush();
}


//***************************************************************************
// VMAIntervalSet
//***************************************************************************

// Given an interval x to insert or delete from the interval set, the
// following cases are possible where x is represented by <> and
// existing interval set by {}.  
//
// We first find lb and ub such that: (lb <= x < ub) and lb != x.
//
//   0) lb interval already contains x
//        {lb <> }
//        {lb <> } ...
//        ... {lb <> }
//        ... {lb <> } ...
//   1) x is a mutually exclusive interval:
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
//   5) x subsumes all intervals
//        < {} ... {} >


// insert: See above for cases
std::pair<VMAIntervalSet::iterator, bool>
VMAIntervalSet::insert(const VMAIntervalSet::value_type& x)
{
  DIAG_DevMsgIf(DBG, "VMAIntervalSet::insert [begin]\n"
		<< "  this: " << toString() << endl 
		<< "  add : " << x.toString());
  
  std::pair<VMAIntervalSet::iterator, bool> ret(end(), false);

  if (x.empty()) {
    return ret;
  }

  // find [lb, ub) where lb is the first element !< x and ub is the
  // first element > x.  IOW, since intervals cannot be duplicated, if
  // lb != end() then (lb >= x < ub)
  std::pair<VMAIntervalSet::iterator, VMAIntervalSet::iterator> lu = 
    equal_range(x);
  VMAIntervalSet::iterator lb = lu.first;
  VMAIntervalSet::iterator ub = lu.second;

  // find (lb <= x < ub) such that lb != x
  if (lb == end()) {
    if (!empty()) { // all elements are less than x
      lb = --end();
    }
  }
  else {
    if (lb == begin()) { // no element is less than x
      lb = end();
    }
    else {
      --lb;
    }
  }
  
  // Detect the appropriate case.  Note that case 0 is a NOP.
  VMA lb_beg = lb->beg();
  VMA ub_end = ub->end();

  if (lb != end() && lb->contains(x)) {
    // Case 0: do nothing
  }
  else if (lb == end() && ub == end()) { // size >= 1
    if (empty()) {
      // Case 1a: insert x
      ret = My_t::insert(x);
    }
    else {
      // Case 5: erase everything and insert x
      My_t::clear();
      ret = My_t::insert(x);
    }
  }
  else if (lb == end()) {
    if (x.end() < ub->beg()) {
      // Case 1b: insert x
      ret = My_t::insert(x);
    }
    else {
      // Case 3a: erase [begin(), ub + 1); insert [x.beg(), ub->end())
      VMAIntervalSet::iterator end = ub;
      My_t::erase(begin(), ++end);
      ret = My_t::insert( VMAInterval(x.beg(), ub_end) );
    }
  }
  else if (ub == end()) {
    if (lb->end() < x.beg()) {
      // Case 1c: insert x
      ret = My_t::insert(x);
    }
    else {
      // Case 2a: erase [lb, end()); insert [lb->beg(), x.end())
      My_t::erase(lb, end());
      ret = My_t::insert( VMAInterval(lb_beg, x.end()) );
    }
  }
  else {
    if (lb->end() < x.beg() && x.end() < ub->beg()) {
      // Case 1d: insert x
      ret = My_t::insert(x);
    }
    else if (x.beg() <= lb->end() && x.end() < ub->beg()) {
      // Case 2b: erase [lb, ub); insert [lb->beg(), x.end())
      My_t::erase(lb, ub);
      ret = My_t::insert( VMAInterval(lb_beg, x.end()) );
    }
    else if (lb->end() < x.beg() && ub->beg() <= x.end()) {
      // Case 3b: erase [lb + 1, ub + 1): insert [x.beg(), ub->end())
      VMAIntervalSet::iterator beg = lb;
      VMAIntervalSet::iterator end = ub;
      My_t::erase(++beg, ++end);
      ret = My_t::insert( VMAInterval(x.beg(), ub_end) );
    }
    else {
      // Case 4: erase [lb, ub + 1); insert [lb->beg(), ub->end()).
      VMAIntervalSet::iterator end = ub;
      My_t::erase(lb, ++end);
      ret = My_t::insert( VMAInterval(lb_beg, ub_end) );
    }
  }

  DIAG_DevMsgIf(DBG, "VMAIntervalSet::insert [end]\n"
		<< "  this: " << toString() << endl);
  
  return ret;
}


// erase: See above for cases
void
VMAIntervalSet::erase(const VMAIntervalSet::key_type& x)
{
  if (x.empty()) {
    return;
  }
  
  // find [lb, ub) where lb is the first element !< x and ub is the
  // first element > x.  IOW, since intervals cannot be duplicated, if
  // lb != end() then (lb = x < ub) and lb equiv x.
  std::pair<VMAIntervalSet::iterator, VMAIntervalSet::iterator> lu = 
    equal_range(x);
  VMAIntervalSet::iterator lb = lu.first;
  VMAIntervalSet::iterator ub = lu.second;

  // find (lb <= x < ub) such that lb != x
  if (lb == end()) {
    if (!empty()) { // all elements are less than x
      lb = --end();
    }
  }
  else {
    if (lb == begin()) { // no element is less than x
      lb = end();
    }
    else {
      --lb;
    }
  }
  
  // Detect the appropriate case.  Note that we do not have to
  // explicitly consider Case 1 since it amounts to a NOP.
  VMA lb_beg = lb->beg();
  VMA ub_end = ub->end();

  if (lb != end() && lb->contains(x)) {
    // Case 0: split interval: erase [lb, ub); 
    //   insert [lb->beg(), x.beg()), [x.end(), lb->end())
    VMA lb_end = lb->end();
    My_t::erase(lb, ub);
    if (lb_beg < x.beg()) {
      My_t::insert( VMAInterval(lb_beg, x.beg()) );
    }
    if (lb_end > x.end()) {
      My_t::insert( VMAInterval(x.end(), lb_end) );
    }
  }
  else if (lb == end() && ub == end()) { // size >= 1
    if (!empty()) {
      // Case 5: erase everything
      My_t::clear();
    }
  }
  else if (lb == end()) {
    if ( !(x.end() < ub->beg()) ) {
      // Case 3a: erase [begin(), ub + 1); insert [x.end(), ub->end())
      VMAIntervalSet::iterator end = ub;
      My_t::erase(begin(), ++end);
      if (x.end() < ub_end) {
	My_t::insert( VMAInterval(x.end(), ub_end) );
      }
    }
  }
  else if (ub == end()) {
    if ( !(lb->end() < x.beg()) ) {
      // Case 2a: erase [lb, end()); insert [lb->beg(), x.beg())
      My_t::erase(lb, end());
      if (lb_beg < x.beg()) {
	My_t::insert( VMAInterval(lb_beg, x.beg()) );
      }
    }
  }
  else {
    if (x.beg() <= lb->end() && x.end() < ub->beg()) {
      // Case 2b: erase [lb, ub); insert [lb->beg(), x.beg())
      My_t::erase(lb, ub);
      if (lb_beg < x.beg()) {
	My_t::insert( VMAInterval(lb_beg, x.beg()) );
      }
    }
    else if (lb->end() < x.beg() && ub->beg() <= x.end()) {
      // Case 3b: erase [lb + 1, ub + 1): insert [x.end(), ub->end())
      VMAIntervalSet::iterator beg = lb;
      VMAIntervalSet::iterator end = ub;
      My_t::erase(++lb, ++ub);
      if (x.end() < ub_end) {
	My_t::insert( VMAInterval(x.end(), ub_end) );
      }
    }
    else if ( !(lb->end() < x.beg() && x.end() < ub->beg()) ) {
      // Case 4: erase [lb, ub + 1); 
      //   insert [lb->beg(), x.beg()) and [x.end(), ub->end()).
      VMAIntervalSet::iterator end = ub;
      My_t::erase(lb, ++end);
      My_t::insert( VMAInterval(lb_beg, x.beg()) );
      My_t::insert( VMAInterval(x.end(), ub_end) );
    }
  }
}


void
VMAIntervalSet::merge(const VMAIntervalSet& x)
{
  for (const_iterator it = x.begin(); it != x.end(); ++it) {
    insert(*it); // the overloaded insert
  }
}


string
VMAIntervalSet::toString() const
{
  string self = "{";
  bool needSeparator = false;
  for (const_iterator it = this->begin(); it != this->end(); ++it) {
    if (needSeparator) { self += " "; }
    self += (*it).toString();
    needSeparator = true;
  }
  self += "}";
  return self;
}


void
VMAIntervalSet::fromString(const char* formattedstr)
{
  const char* s = formattedstr;
  const char* p = formattedstr;
  if (!p || p[0] == '\0') { return; }
  
  // skip '{'
  DIAG_Assert(*p == '{', DIAG_UnexpectedInput << "'" << s << "'");
  p++;

  // find intervals: p points to '[' and q points to ')'
  const char* q = p;
  while ( (p = strchr(q, '[')) ) {
    VMAInterval vmaint(p);
    insert(vmaint); // the overloaded insert
    q = strchr(p, ')'); // q point
  }
  q++;
  
  // skip '}'
  DIAG_Assert(*q == '}', DIAG_UnexpectedInput << "'" << s << "'");
  p++;
}


std::ostream&
VMAIntervalSet::dump(std::ostream& os) const
{
  bool needSeparator = false;
  os << "{";
  for (const_iterator it = this->begin(); it != this->end(); ++it) {
    if (needSeparator) { os << " "; }
    (*it).dump(os);
    needSeparator = true;
  }
  os << "}";
  return os;
}


void
VMAIntervalSet::ddump() const
{
  dump(std::cout);
  std::cout.flush();
}


//***************************************************************************
// VMAIntervalMap
//***************************************************************************


