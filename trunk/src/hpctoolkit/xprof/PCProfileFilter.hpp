// $Id$
// -*-C++-*-
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
//    PCProfileFilter.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    See, in particular, the comments associated with 'PCProfile'.
//
//***************************************************************************

#ifndef PCProfileFilter_H 
#define PCProfileFilter_H

//************************* System Include Files ****************************

#include <vector>
#include <list>

#include "inttypes.h"

//*************************** User Include Files ****************************

#include <include/general.h>

#include <lib/ISA/ISATypes.h>
#include <lib/support/String.h>
#include <lib/support/Assertion.h>

//*************************** Forward Declarations ***************************

class PCProfileFilter;
class MetricFilter;
class PCFilter;

// Some useful containers
typedef std::list<PCProfileFilter*>         PCProfileFilterList;
typedef PCProfileFilterList::iterator       PCProfileFilterListIt;
typedef PCProfileFilterList::const_iterator PCProfileFilterListCIt;

class PCProfileMetric;

//****************************************************************************
// PCProfileFilter
//****************************************************************************

// 'PCProfileFilter' is a filter that implicitly defines a derived
// metric (e.g. 'ProfileMetric') by specifing some combination of raw
// metrics and pcs from a 'PCProfile'.
// Currently, filters are strictly divided between metric and pc filters.
class PCProfileFilter
{
public:
  PCProfileFilter(MetricFilter* x = NULL, PCFilter* y = NULL)
    : mfilt(x), pcfilt(y) { }
  virtual ~PCProfileFilter() { }

  // Name, Description: The name and a description of what this filter
  // computes.
  const char* GetName()        const { return name; }
  const char* GetDescription() const { return description; }

  void SetName(const char* s)          { name = s; }
  void SetDescription(const char* s)   { description = s; }

  // Access to the various sub-filters.  (These intentionally return
  // non-const pointers!)
  MetricFilter* GetMetricFilter() const { return mfilt; }
  PCFilter* GetPCFilter() const { return pcfilt; }
  
  void Dump(std::ostream& o = std::cerr);
  void DDump(); 
  
private:
  // Should not be used 
  PCProfileFilter(const PCProfileFilter& p) { }
  PCProfileFilter& operator=(const PCProfileFilter& p) { return *this; }
  
protected:
private:
  String name;
  String description;

  MetricFilter* mfilt;
  PCFilter* pcfilt;
};


//****************************************************************************
// MetricFilter
//****************************************************************************

// MetricFilter: An abstract base class providing an interface for
// standard and user-defined metric filters.  A MetricFilter divides
// metrics into two sets sets, 'in' and 'out'. 
class MetricFilter {
public:
  MetricFilter() { }
  virtual ~MetricFilter() { }
  
  // Returns true if 'm' is within the 'in' set; false otherwise.
  virtual bool operator()(const PCProfileMetric* m) = 0;

private:
};


//****************************************************************************
// PCFilter
//****************************************************************************

// PCFilter: An abstract base class providing an interface for
// standard and user-defined PC filters.  A PCFilter divides
// PC into two sets sets, 'in' and 'out'. 
class PCFilter {
public:
  PCFilter() { }
  virtual ~PCFilter() { }
  
  // Returns true if 'pc' is within the 'in' set; false otherwise.
  virtual bool operator()(const Addr pc) = 0;

private:
};


//****************************************************************************
// InsnClassExpr
//****************************************************************************

// InsnClassExpr: A common 'PCFilter' will be an instruction class
// filter (cf. 'InsnFilter').  This represent a disjunctive expression
// of instruction classes that can be used with an 'InsnFilter'.

class InsnClassExpr {
public:
  typedef uint32_t bitvec_t;
    
public:
  // A 'InsnClassExpr' can be created using the bit definitions above.
  InsnClassExpr(bitvec_t bv = 0) : bits(bv) { }
  virtual ~InsnClassExpr() { }
  
  InsnClassExpr(const InsnClassExpr& x) { *this = x; }
  InsnClassExpr& operator=(const InsnClassExpr& x) { 
    bits = x.bits; 
    return *this;
  }
  
  // IsValid: If no bits are set, this must be an invalid expression
  bool IsValid() const { return bits != 0; }
  
  // IsSet: Tests to see if all the specified bits are set
  bool IsSet(const bitvec_t bv) const {
    return (bits & bv) == bv;
  }
  bool IsSet(const InsnClassExpr& m) const {
    return (bits & m.bits) == m.bits; 
  }
  // Set: Set all the specified bits
  void Set(const bitvec_t bv) {
    bits = bits | bv;
  }
  void Set(const InsnClassExpr& m) {
    bits = bits | m.bits;
  }
  // Unset: Clears all the specified bits
  void Unset(const bitvec_t bv) {
    bits = bits & ~bv;
  }
  void Unset(const InsnClassExpr& m) {
    bits = bits & ~(m.bits);
  }
  
  void Dump(std::ostream& o = std::cerr);
  void DDump();
  
private:
  
protected:
private:  
  bitvec_t bits;
};

//****************************************************************************
// InsnFilter
//****************************************************************************

class LoadModule; // FIXME

// InsnFilter: Divides PCs into two sets by Alpha instruction class.
class InsnFilter : public PCFilter {
public:
  InsnFilter(InsnClassExpr* expr, LoadModule* lm) { }
  virtual ~InsnFilter() { }
  
  // Returns true if 'pc' satisfies 'expr'; false otherwise.
  virtual bool operator()(Addr pc);

private:
};


//****************************************************************************

#endif 
