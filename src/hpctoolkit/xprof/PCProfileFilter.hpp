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

#endif 
