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
//    PCProfileMetric.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    See, in particular, the comments associated with 'PCProfile'.
//
//***************************************************************************

#ifndef PCProfileMetric_H 
#define PCProfileMetric_H

//************************* System Include Files ****************************

#include <map>

#ifdef NO_STD_CHEADERS
# include <limits.h>
#else
# include <climits>
#endif

//*************************** User Include Files ****************************

#include <include/general.h>

#include "PCProfileFilter.h"

#include <lib/ISA/ISATypes.h>
#include <lib/support/String.h>
#include <lib/support/Assertion.h>

//*************************** Forward Declarations ***************************

// 'PCProfileDatum' holds a single profile count or statistic. 
typedef ulong PCProfileDatum; 
#define PCProfileDatum_NIL 0 /* no data is present */

class PCSet; // FIXME: share with PCProfile.h

//****************************************************************************
// PCProfileMetric
//****************************************************************************

// 'PCProfileMetric' defines a profiling metric and all raw data
// resulting from one profiling run in a [pc->datum] map.  Since
// profiling data for mutiple metrics often results in a 'sparse
// matrix', data values of 0 (PCProfileDatum_NIL) are not explicitly
// stored.  Note that in the case of VLIW machines, PCs are really
// 'operation PCs', that is, a single value that combines the PC and
// an offset identifying the operation within the VLIW packet.  The
// 'operation PC' should follow ISA class conventions. (Creaters of a
// 'PCProfileMetric' should therefore use ISA::ConvertPCToOpPC to
// generate the 'operation PCs'.)
// see: 'PCProfileMetric_MapIterator'
class PCProfileMetric
{
private:
  typedef std::map<Addr, PCProfileDatum>        PCToPCProfileDatumMap;
  typedef PCToPCProfileDatumMap::value_type     PCToPCProfileDatumMapVal;
  typedef PCToPCProfileDatumMap::iterator       PCToPCProfileDatumMapIt;
  typedef PCToPCProfileDatumMap::const_iterator PCToPCProfileDatumMapCIt;

public:
  PCProfileMetric()
    : total(0), period (0), txtStart(0), txtSz(0)
    { /* map does not need to be initialized */ }
  virtual ~PCProfileMetric() { }
  
  // Name, Description: The metric name and a description
  // TotalCount: The sum of all raw data for this metric
  // Period: The sampling period (whether event or instruction based)
  // TxtStart, TxtSz: Beginning of the text segment and the text segment size
  const char*    GetName()        const { return name; }
  const char*    GetDescription() const { return description; }
  PCProfileDatum GetTotalCount()  const { return total; }
  ulong          GetPeriod()      const { return period; }
  Addr           GetTxtStart()    const { return txtStart; }
  Addr           GetTxtSz()       const { return txtSz; }
  
  void SetName(const char* s)          { name = s; }
  void SetDescription(const char* s)   { description = s; }
  void SetTotalCount(PCProfileDatum d) { total = d; }
  void SetPeriod(ulong p)              { period = p; }
  void SetTxtStart(Addr a)             { txtStart = a; }
  void SetTxtSz(Addr a)                { txtSz = a; }

  // 'GetSz': The number of entries (note: this is not necessarily the
  // number of instructions or PC values in the text segment).
  suint GetSz() const { return map.size(); }

  // find/insert by PC value (GetTxtStart() <= PC <= GetTxtStart()+GetTxtSz())
  // 'Find': return datum for 'pc'; PCProfileDatum_NIL if not found.
  // (Note that this means one cannot distinguish between a dataset
  // resulting from the insertion of <pc, 0> and a dataset in which no
  // insertion was performed for the same pc.  However, this should
  // not be a problem.)
  PCProfileDatum Find(Addr pc) const {
    PCToPCProfileDatumMapCIt it = map.find(pc);
    if (it == map.end()) { return PCProfileDatum_NIL; } 
    else { return ((*it).second); }
  }
  void Insert(Addr pc, PCProfileDatum& d) {
    if (d != PCProfileDatum_NIL) {
      map.insert(PCToPCProfileDatumMapVal(pc, d)); // do not add duplicates!
    }
  }

  // Filter(): Returns a set of PCs metrics that pass the filter
  // (i.e., every PC 'pc' for which 'filter' returns true.)  User
  // becomes responsible for freeing memory.
  PCSet* Filter(PCFilter* filter) const;

  void Dump(std::ostream& o = std::cerr);
  void DDump(); 

private:
  // Should not be used  
  PCProfileMetric(const PCProfileMetric& m) { }
  PCProfileMetric& operator=(const PCProfileMetric& m) { return *this; }
  
  friend class PCProfileMetric_MapIterator;
  
protected:
private:  
  String name;
  String description;
  
  PCProfileDatum total;    // sum across all pc values recorded for this event
  ulong          period;   // sampling period
  Addr           txtStart; // beginning of text segment 
  Addr           txtSz;    // size of text segment
  
  PCToPCProfileDatumMap map; // map of sampling data
};


// 'PCProfileMetric_MapIterator' iterates over the [pc->datum] map of a
// 'PCProfileMetric'.  Because data values of 0 (PCProfileDatum_NIL)
// are not explicitly stored, they will not appear in the iteration.
class PCProfileMetric_MapIterator
{
public:
  PCProfileMetric_MapIterator(const PCProfileMetric& x) : m(x) {
    Reset();
  }
  virtual ~PCProfileMetric_MapIterator() { }

  Addr           CurrentSrc()    { return (*it).first; }
  PCProfileDatum CurrentTarget() { return (*it).second; }

  void operator++()    { it++; } // prefix
  void operator++(int) { ++it; } // postfix

  bool IsValid() const { return it != m.map.end(); } 
  bool IsEmpty() const { return it == m.map.end(); }
  
  // Reset and prepare for iteration again
  void Reset()  { it = m.map.begin(); }
  
private:
  // Should not be used  
  PCProfileMetric_MapIterator();
  PCProfileMetric_MapIterator(const PCProfileMetric_MapIterator& x);
  PCProfileMetric_MapIterator& operator=(const PCProfileMetric_MapIterator& x) 
    { return *this; }

protected:
private:
  const PCProfileMetric& m;
  PCProfileMetric::PCToPCProfileDatumMapCIt it;
};


//****************************************************************************

#endif 

