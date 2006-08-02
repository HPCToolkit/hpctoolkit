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
//    PCProfile.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    See, in particular, the comments associated with 'PCProfile'.
//
//***************************************************************************

#ifndef PCProfile_H 
#define PCProfile_H

//************************* System Include Files ****************************

#include <vector>
#include <list>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "PCProfileMetric.hpp"
#include "PCProfileFilter.hpp"

#include <lib/isa/ISA.hpp>

#include <lib/support/String.hpp>

//*************************** Forward Declarations ***************************

// Some useful containers
typedef std::list<PCProfileMetric*>         PCProfileMetricList;
typedef PCProfileMetricList::iterator       PCProfileMetricListIt;
typedef PCProfileMetricList::const_iterator PCProfileMetricListCIt;

typedef std::vector<PCProfileMetric*>      PCProfileMetricVec;
typedef PCProfileMetricVec::iterator       PCProfileMetricVecIt;
typedef PCProfileMetricVec::const_iterator PCProfileMetricVecCIt;

typedef std::vector<VMA>     PCVec;
typedef PCVec::iterator       PCVecIt;
typedef PCVec::const_iterator PCVecCIt;

//****************************************************************************
// PCProfileMetricSet
//****************************************************************************

// 'PCProfileMetricSet' is a set of PCProfileMetrics.  It can be used
// to represent all metrics in raw profile data or some collection of
// raw metrics forming a derived metric.  Filters can be applied to
// yield a new set.  A set contains an ISA, which its metrics point
// to, primarily for conversion between 'pcs' and 'operation pcs'.
// Note that the ISA is reference counted.
class PCProfileMetricSet
{
private:
  
public:
  // Constructor: reserves at least 'sz' slots; 'isa_' is *reference counted*
  PCProfileMetricSet(ISA* isa_, suint sz = 16); 
  virtual ~PCProfileMetricSet(); 
  
  // Access to metrics (0-based).  When a set is created, space for
  // slots is only reserved.  To add slots, one must use 'Add' or
  // 'SetSz'.  Once slots exist, they can be randomly accessed via
  // 'Assign' and 'operator[]'.
  const PCProfileMetric* Index(suint i) const { return metricVec[i]; }
  const PCProfileMetric* operator[](suint i) const { return metricVec[i]; }

  void Assign(suint i, const PCProfileMetric* m) { 
    metricVec[i] = const_cast<PCProfileMetric*>(m); // assume ownership of m
  }
  PCProfileMetric*& operator[](suint i) { return metricVec[i]; }

  void Add(const PCProfileMetric* m) {
    metricVec.push_back(const_cast<PCProfileMetric*>(m));
  }

  suint GetSz() const { return metricVec.size(); }
  void SetSz(suint sz) { metricVec.resize(sz); }  

  void Clear() { metricVec.clear(); }


  // 'GetISA': Note: A user must call ISA::Attach() if this is more
  // than a momentary reference!
  ISA* GetISA() const { return isa; }
  
  // DataExists(): Does non-NIL data exist for some metric at the
  // operation designated by 'pc' and 'opIndex'?  If yes, returns the
  // index of the first metric with non-nil data (forward iteration
  // from 0 to size); otherwise, returns negative.  
  sint DataExists(VMA pc, ushort opIndex) const;

  // Filter(): Returns a new, non-null but possibly empty, set of
  // metrics that pass the filter (i.e., every metric metric 'm' for
  // which 'filter' returns true.)  User becomes responsible for
  // freeing memory.
  PCProfileMetricSet* Filter(MetricFilter* filter) const;
  
  void Dump(std::ostream& o = std::cerr);
  void DDump(); 
  
private:
  // Should not be used 
  PCProfileMetricSet(const PCProfileMetricSet& p) { }
  PCProfileMetricSet& operator=(const PCProfileMetricSet& p) { return *this; }

  friend class PCProfileMetricSetIterator;
  
protected:
private:
  
  // We lie a little about this being a 'set' and use a vector for
  // implementation.  Set indexing (random access) may be convenient;
  // and any possible resizing should be cheap b/c these sets should
  // be relatively small.
  PCProfileMetricVec metricVec;
  ISA* isa; 
};


// 'PCProfileMetricSetIterator' iterates over all 'PCProfileMetric'
// within a 'PCProfile'.
class PCProfileMetricSetIterator
{
public:
  PCProfileMetricSetIterator(const PCProfileMetricSet& x) : s(x) {
    Reset();
  }
  virtual ~PCProfileMetricSetIterator() { }

  PCProfileMetric* Current() const { return (*it); }

  void operator++()    { it++; } // prefix
  void operator++(int) { ++it; } // postfix

  bool IsValid() const { return it != s.metricVec.end(); } 
  bool IsEmpty() const { return it == s.metricVec.end(); }

  // Reset and prepare for iteration again
  void Reset()  { it = s.metricVec.begin(); }
  
private:
  // Should not be used 
  PCProfileMetricSetIterator();
  PCProfileMetricSetIterator(const PCProfileMetricSetIterator& x);
  PCProfileMetricSetIterator& operator=(const PCProfileMetricSetIterator& x) 
    { return *this; }

protected:
private:
  const PCProfileMetricSet& s;
  PCProfileMetricVecCIt it;
};

//****************************************************************************
// PCProfile
//****************************************************************************

// 'PCProfile' represents data resulting from one or more different
// profile runs.  A 'PCProfile' is a 'PCProfileMetricSet' and
// consequently contains a set of metrics, each with their own
// [pc->count] map.  'PCProfile' is abstract enough to represent
// profile data from event-sampling systems such as SGI's ssrun or
// instruction-sampling systems such as DEC/Compaq/HP's DCPI
// ProfileMe.
//
// Because each metric contains its own sparse [pc->count] map (0
// counts are not recorded) it is difficult to know in advance for
// which PCs there is a non-zero count for at least one metric.
// Because of this, a 'PCProfile' also contains a list of PCs at which
// at least one metric contains non-zero counts.
//
class PCProfile : public PCProfileMetricSet
{
public:
  PCProfile(ISA* isa_, suint sz = 16);
  virtual ~PCProfile();
  
  // ProfiledFile: the name of the profiled program image
  // HdrInfo: a copy of the unparsed header info. 
  const char* GetProfiledFile() const { return profiledFile; }
  const char* GetHdrInfo()      const { return fHdrInfo; }
  
  void SetProfiledFile(const char* s)  { profiledFile = s; }
  void SetHdrInfo(const char* s)       { fHdrInfo = s; }
  
  // Text start and size (redundant).  Note: all metrics for one
  // profile should have identical values for
  // PCProfileMetric::GetTxtStart() and PCProfileMetric::GetTxtSz()
  VMA GetTxtStart() const { return (GetSz()) ? Index(0)->GetTxtStart() : 0; }
  VMA GetTxtSz()    const { return (GetSz()) ? Index(0)->GetTxtSz() : 0; }
  
  // Access to metrics (redundant)
  const PCProfileMetric* GetMetric(suint i) const { return Index(i); }
  void SetMetric(suint i, const PCProfileMetric* m) { Assign(i, m); }
  void AddMetric(const PCProfileMetric* m) { Add(m); }
  suint GetNumMetrics() const { return GetSz(); }
  void SetNumMetrics(suint sz) { SetSz(sz); }

  // Access to PCs containing non-zero profiling info
  suint GetNumPCs() { return pcVec.size(); }
  void AddPC(VMA pc, ushort opIndex); // be careful: should be no duplicates

  void Dump(std::ostream& o = std::cerr);
  void DDump(); 
  
private:
  // Should not be used 
  PCProfile(const PCProfile& p);
  PCProfile& operator=(const PCProfile& p) { return *this; }
  
  friend class PCProfile_PCIterator;

protected:
private:
  String profiledFile; // name of profiled file
  String fHdrInfo;     // unparsed file header info

  PCVec pcVec; // PCs for which some metric has non-zero data
};


// PCProfile_PCIterator
class PCProfile_PCIterator
{
public:
  PCProfile_PCIterator(const PCProfile& x) : p(x) {
    Reset();
  }
  virtual ~PCProfile_PCIterator() { }

  // Note: This is the 'operation PC' and may not actually be the true
  // PC!  cf. ISA::ConvertOpPCToPC(...).
  VMA Current() const { return (*it); }

  void operator++()    { it++; } // prefix
  void operator++(int) { ++it; } // postfix

  bool IsValid() const { return it != p.pcVec.end(); } 
  bool IsEmpty() const { return it == p.pcVec.end(); }

  // Reset and prepare for iteration again
  void Reset()  { it = p.pcVec.begin(); }
  
private:
  // Should not be used 
  PCProfile_PCIterator();
  PCProfile_PCIterator(const PCProfile_PCIterator& x);
  PCProfile_PCIterator& operator=(const PCProfile_PCIterator& x) 
    { return *this; }

protected:
private:
  const PCProfile& p;
  PCVecCIt it;
};

//****************************************************************************
// PCProfileVec
//****************************************************************************

// 'PCProfileVec' is a vector of 'PCProfileDatum.'  A value can be
// associated with the vector.  A 'PCProfileVec' can be used to record
// a cross-section of several 'PCProfileMetric'.  E.g. it could
// contain all counts at a particlar PC (or line number) or represent
// statistics accross all metrics.

class PCProfileVec
{
public:
  PCProfileVec(suint sz);
  virtual ~PCProfileVec() { }

  suint GetDatum() const { return datum; }
  suint GetSz()    const { return vec.size(); }

  void SetDatum(suint d) { datum = d; }

  bool IsZeroed(); 
  
  PCProfileDatum& operator[](suint i) { return vec[i]; }

  void Dump(std::ostream& o = std::cerr);
  void DDump(); 
  
private:
  // Should not be used  
  PCProfileVec() { }
  PCProfileVec(const PCProfileVec& v) { }
  PCProfileVec& operator=(const PCProfileVec& v) { return *this; }
  
protected:
private:  
  suint datum; // datum to associate with vec (e.g. pc value, line no, ptr)
  std::vector<PCProfileDatum> vec;
};


#endif 
