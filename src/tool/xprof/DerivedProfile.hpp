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
//    DerivedProfile.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    See, in particular, the comments associated with 'DerivedProfile'.
//
//***************************************************************************

#ifndef DerivedProfile_H 
#define DerivedProfile_H

//************************* System Include Files ****************************

#include <vector>
#include <list>
#include <string>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "PCProfile.hpp"

#include <lib/isa/ISA.hpp>

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

class DerivedProfileMetric;

// Some useful containers
typedef std::list<DerivedProfileMetric*>         DerivedProfileMetricList;
typedef DerivedProfileMetricList::iterator       DerivedProfileMetricListIt;
typedef DerivedProfileMetricList::const_iterator DerivedProfileMetricListCIt;

typedef std::vector<DerivedProfileMetric*>      DerivedProfileMetricVec;
typedef DerivedProfileMetricVec::iterator       DerivedProfileMetricVecIt;
typedef DerivedProfileMetricVec::const_iterator DerivedProfileMetricVecCIt;

//****************************************************************************
// DerivedProfile
//****************************************************************************

// 'DerivedProfile' represents metrics derived in some way from the
// raw profiling information in *one* 'PCProfile'.  It contains a set
// of 'DerivedProfileMetrics' (which themselves contain a
// 'PCProfileMetricSet').  The derived metric information is specified
// and obtained by applying certain metric and pc filters to the raw
// profiling information.  In particular, there will be one
// 'DerivedProfileMetric' for each 'PCProfileFilter' in the filter
// list.  Note that a 'DerivedProfileMetric' may be an emtpy set,
// i.e. the corresponding filter was never satisfied.
class DerivedProfile
{
public:
  DerivedProfile();
  DerivedProfile(const PCProfile* pcprof_, 
		 const PCProfileFilterList* filtlist);
  virtual ~DerivedProfile();

  // Create: Does not assume ownership of 'pcprof_' or 'filtlist' but
  // does save a pointer to the former.  If filtlist is NULL, an
  // 'identity' filter is assumed.
  void Create(const PCProfile* pcprof_, const PCProfileFilterList* filtlist);

  // GetPCProfile: Returns 'PCProfile', the underlying raw data
  const PCProfile* GetPCProfile() const { return pcprof; }
  
  // Access to metrics (0 based): When a metric set is created, space
  // for slots is only reserved.  To add slots, one must use
  // 'AddMetric' or 'SetNumMetrics'.  Once slots exist, they can be
  // randomly accessed via 'SetMetric', etc.
  const DerivedProfileMetric* GetMetric(unsigned int i) const { 
    return metricVec[i]; 
  }
  void SetMetric(unsigned int i, const DerivedProfileMetric* m) { 
    metricVec[i] = const_cast<DerivedProfileMetric*>(m); // assume ownership
  }
  void AddMetric(const DerivedProfileMetric* m) { 
    metricVec.push_back(const_cast<DerivedProfileMetric*>(m)); 
  }
  unsigned int GetNumMetrics() const { return metricVec.size(); }
  void SetNumMetrics(unsigned int sz) { metricVec.resize(sz); }

  void Dump(std::ostream& o = std::cerr);
  void DDump(); 

private:
  // Should not be used 
  DerivedProfile(const DerivedProfile& p) { }
  DerivedProfile& operator=(const DerivedProfile& p) { return *this; }
  
  friend class DerivedProfile_MetricIterator;
  
protected:
private:
  const PCProfile* pcprof;
  
  // We lie a little about this being a 'set' and use a vector for
  // implementation.  Set indexing (random access) may be convenient;
  // and any possible resizing should be cheap b/c these sets should
  // be relatively small.
  DerivedProfileMetricVec metricVec;
};


// 'DerivedProfile_MetricIterator' iterates over all 'DerivedProfileMetric'
// within a 'DerivedProfile'.
class DerivedProfile_MetricIterator
{
public:
  DerivedProfile_MetricIterator(const DerivedProfile& x) : p(x) {
    Reset();
  }
  virtual ~DerivedProfile_MetricIterator() { }

  DerivedProfileMetric* Current() const { return (*it); }

  void operator++()    { it++; } // prefix
  void operator++(int) { ++it; } // postfix

  bool IsValid() const { return it != p.metricVec.end(); } 
  bool IsEmpty() const { return it == p.metricVec.end(); }

  // Reset and prepare for iteration again
  void Reset()  { it = p.metricVec.begin(); }
  
private:
  // Should not be used 
  DerivedProfile_MetricIterator();
  DerivedProfile_MetricIterator(const DerivedProfile_MetricIterator& x);
  DerivedProfile_MetricIterator& operator=(const DerivedProfile_MetricIterator& x) 
    { return *this; }

protected:
private:
  const DerivedProfile& p;
  DerivedProfileMetricVecCIt it;
};


//****************************************************************************
// DerivedProfileMetric
//****************************************************************************

// 'DerivedProfileMetric' represents a metric derived in some way from
// the raw profiling information in *one* 'PCProfile'.  It contains a
// 'PCProfileMetricSet' pointing to one or more raw metrics within the
// 'PCProfile' that *additively* represent this derived metric.
class DerivedProfileMetric
{
public:
  // One can create an empty object or quickly create an object
  // initialized with an existing set.  As a special case, one can
  // quickly create a fully initialized object with a one-element set;
  // the derived name and description will come directly from the
  // 'PCProfileMetric'.  We assume ownership of the metric set
  // container (but not the contents -- the 'PCProfileMetric's).
  DerivedProfileMetric(const PCProfileMetricSet* s = NULL);
  DerivedProfileMetric(const PCProfileMetric* m);
  virtual ~DerivedProfileMetric();
  
  // Name, Description: The metric name (high level name) and a description
  // NativeName: A name that is a combination of the raw metrics
  // Period: The sampling period (whether event or instruction based)
  const std::string& GetName()        const { return name; }
  const std::string& GetNativeName()  const { return nativeName; }
  const std::string& GetDescription() const { return description; }
  ulong              GetPeriod()      const { return period; }

  void SetName(const char* s)        { name = s; }
  void SetName(const std::string& s) { name = s; }

  void SetNativeName(const char* s)        { nativeName = s; }
  void SetNativeName(const std::string& s) { nativeName = s; }

  void SetDescription(const char* s)        { description = s; }
  void SetDescription(const std::string& s) { description = s; }

  void SetPeriod(ulong p) { period = p; }

  // MetricSet: The set of 'PCProfileMetric's
  const PCProfileMetricSet* GetMetricSet() const { return mset; }
  void SetMetricSet(const PCProfileMetricSet* s) { 
    mset = const_cast<PCProfileMetricSet*>(s); 
  } 

  // PCSet: The set of relevant PCs (may be NULL or empty)
  const PCSet* GetPCSet() const { return pcset; }
  // A special function for saving some memory; use with caution
  void MakeDerivedPCSetCoterminousWithPCSet() { delete pcset; pcset = NULL; }

  bool FindPC(VMA pc, ushort opIndex) { 
    if (pcset) {
      VMA oppc = mset->GetISA()->convertVMAToOpVMA(pc, opIndex);
      PCSetIt it = pcset->find(oppc);
      return (it != pcset->end());
    } else {
      return (mset->DataExists(pc, opIndex) >= 0);
    }
  }
  void InsertPC(VMA pc, ushort opIndex) {
    DIAG_Assert(pcset, "");
    VMA oppc = mset->GetISA()->convertVMAToOpVMA(pc, opIndex);
    pcset->insert(oppc); // do not add duplicates!
  }

  void Dump(std::ostream& o = std::cerr);
  void DDump(); 

private:
  // Should not be used  
  DerivedProfileMetric(const DerivedProfileMetric& m) { }
  DerivedProfileMetric& operator=(const DerivedProfileMetric& m) { return *this; }

  void Ctor(const PCProfileMetricSet* s);

  
protected:
private:
  std::string name;
  std::string nativeName;
  std::string description;
  ulong period; // sampling period

  PCProfileMetricSet* mset; // we own the set container, but not the contents!
  PCSet* pcset;  // the set of PCs relevant for this metric.  If NULL, 
                 // the set is coterminous with the set of PCs within 'mset'
};

#endif 
