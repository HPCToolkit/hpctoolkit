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
#include <map>

#ifdef NO_STD_CHEADERS
# include <limits.h>
#else
# include <climits>
#endif

//*************************** User Include Files ****************************

#include <include/general.h>

#include <lib/ISA/ISATypes.h>
#include <lib/support/String.h>
#include <lib/support/Assertion.h>

//*************************** Forward Declarations ***************************

// 'PCProfileDatum' holds a single profile count or statistic. 
typedef unsigned long PCProfileDatum; 
const PCProfileDatum  PCProfileDatum_NIL = ULONG_MAX;

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
  ~PCProfileVec() { }

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


//****************************************************************************

class PCProfileMetric;

// 'PCProfile' can represent data resulting from many different types
// of profile runs.  A 'PCProfile' contains one or more metrics
// ('PCProfileMetric') that represent interesting things that can be
// profiled at runtime.  Given a metric, a 'PCProfile' associates
// profile counts or statistics with a particular PC value.  Thus, a
// 'PCProfile' can be used to represent profile data from
// event-sampling systems such as SGI's ssrun or more sophisticated
// instruction-sampling systems such as Compaq's DCPI ProfileMe.

// Note: If one 'PCProfileMetric' in a 'PCProfile' has an entry at
// PC, then every other 'PCProfileMetric' should have an entry for PC
// (even if the entry is 0).
class PCProfile
{
public:
  ~PCProfile(); 

  const char*    GetProfiledFile() const { return profiledFile; }
  const char*    GetHdrInfo()      const { return fHdrInfo; }
  PCProfileDatum GetTotalCount()   const { return totalCount; }

  void SetProfiledFile(const char* s)  { profiledFile = s; }
  void SetHdrInfo(const char* s)       { fHdrInfo = s; }
  void SetTotalCount(PCProfileDatum d) { totalCount = d; }

  suint            GetMetricVecSz()   const { return metricVec.size(); }
  PCProfileMetric* GetMetric(suint i) const { return metricVec[i]; }

  void Dump(std::ostream& o = std::cerr);
  void DDump(); 
  
private:
  // Should not be used 
  PCProfile() { }
  PCProfile(const PCProfile& p) { }
  PCProfile& operator=(const PCProfile& p) { return *this; }

  // Restrict usage
  PCProfile(suint sz);
  void SetEvent(suint i, PCProfileMetric* m) { 
    metricVec[i] = m; // assume ownership of m
  }

  friend class ProfileReader; 
  
protected:
private:
  String profiledFile;       // name of profiled file
  String fHdrInfo;           // unparsed file header info

  PCProfileDatum totalCount; // sum of metric counts
  
  std::vector<PCProfileMetric*> metricVec;
};

//****************************************************************************

// 'PCProfileMetric' defines a profiling metric and all raw data resulting
// from one profiling run.
// see: 'PCProfileMetricIterator' 
class PCProfileMetric
{
private:
  typedef std::map<Addr, PCProfileDatum>             PCToPCProfileDatumMap;
  typedef std::map<Addr, PCProfileDatum>::value_type PCToPCProfileDatumMapVal;
  typedef std::map<Addr, PCProfileDatum>::iterator   PCToPCProfileDatumMapIt;
  typedef std::map<Addr, PCProfileDatum>::const_iterator 
    PCToPCProfileDatumMapCIt;

public:
  ~PCProfileMetric() { }

  // Name, Description: The metric name and a description
  // TotalCount: The sum of all raw data for this metric
  // Period: The sampling period (whether event or instruction based)
  // TxtStart, TxtSz: Beginning of the text segment and the text segment size
  const char*    GetName()        const { return name; }
  const char*    GetDescription() const { return description; }
  PCProfileDatum GetTotalCount()  const { return total; }
  unsigned int   GetPeriod()      const { return period; }
  Addr           GetTxtStart()    const { return txtStart; }
  Addr           GetTxtSz()       const { return txtSz; }
  
  void SetName(const char* s)          { name = s; }
  void SetDescription(const char* s)   { description = s; }
  void SetTotalCount(PCProfileDatum d) { total = d; }
  void SetPeriod(unsigned int p)       { period = p; }
  void SetTxtStart(Addr a)             { txtStart = a; }
  void SetTxtSz(Addr a)                { txtSz = a; }

  // 'GetSz': The number of entries (note: this is not necessarily the
  // number of PCs in the text segment).
  suint          GetSz() const         { return map.size(); }

  // find/insert by PC value (GetTxtStart() <= PC <= GetTxtStart()+GetTxtSz())
  // 'Find': return datum for 'pc'
  PCProfileDatum Find(suint pc) {
    PCToPCProfileDatumMapIt it = map.find(pc);
    if (it == map.end()) { return PCProfileDatum_NIL; } 
    else { return ((*it).second); }
  }
  void Insert(suint pc, PCProfileDatum& d) {
    BriefAssertion(d != PCProfileDatum_NIL);
    map.insert(PCToPCProfileDatumMapVal(pc, d)); // do not add duplicates!
  }

  void Dump(std::ostream& o = std::cerr);
  void DDump(); 

private:
  // Should not be used  
  PCProfileMetric(const PCProfileMetric& m) { }
  PCProfileMetric& operator=(const PCProfileMetric& m) { return *this; }

  // Restrict usage
  PCProfileMetric()
    : total(0), period (0), txtStart(0), txtSz(0)
  { /* map does not need to be initialized */ }
  
  friend class ProfileReader;
  friend class PCProfileMetricIterator;
  
protected:
private:  
  String name;
  String description;
  
  PCProfileDatum total;    // sum across all pc values recorded for this event
  unsigned int   period;   // sampling period
  Addr           txtStart; // beginning of text segment 
  Addr           txtSz;    // size of text segment
  
  PCToPCProfileDatumMap map; // map of sampling data
};

// 'PCProfileMetricIterator' iterates over a 'PCProfileMetric'
class PCProfileMetricIterator
{
public:
  PCProfileMetricIterator(const PCProfileMetric* m_) : m(m_) {
    Reset();
  }

  void Reset()  { it = m->map.begin(); }
  bool IsDone() { return (it == m->map.end()); }
  
  Addr           CurrentSrc()    { return (*it).first; }
  PCProfileDatum CurrentTarget() { return (*it).second; }
  
  void operator++()    { it++; } // prefix
  void operator++(int) { ++it; } // postfix
  
private:
  // Should not be used  
  PCProfileMetricIterator() { }
  PCProfileMetricIterator(const PCProfileMetricIterator& _m) { }
  PCProfileMetricIterator& operator=(const PCProfileMetricIterator& _m) 
  { return *this; }

protected:
private:
  const PCProfileMetric* m;
  PCProfileMetric::PCToPCProfileDatumMapCIt it;
};

#endif 

