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
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef Analysis_CallPath_MetricComponentsFact_hpp
#define Analysis_CallPath_MetricComponentsFact_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <vector>
#include <stack>
#include <string>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/prof/CallPath-Profile.hpp>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace CallPath {


class MetricComponentsFact
{
public:
  MetricComponentsFact() { }
  
  virtual ~MetricComponentsFact() { }
  
  virtual void
  make(Prof::CallPath::Profile& prof);
  
  virtual bool
  isSeparable(const Prof::CCT::AProcNode* x) = 0;

protected:

  void
  make(Prof::CCT::ANode* node,
       const std::vector<uint>& m_src, const std::vector<uint>& m_dst,
       bool isSeparableCtxt);


  static inline bool
  isTimeMetric(const Prof::Metric::ADesc* mdesc)
  {
    const std::string& nm = mdesc->nameBase();
    return ((nm.find("PAPI_TOT_CYC") == 0) || (nm.find("WALLCLOCK") == 0));
  }


  static const std::string s_sum;   // FIXME: share w/ MetricMgr
  static const std::string s_cfvar; // FIXME: share w/ MetricMgr

  static inline bool
  isDerivedMetric(const Prof::Metric::ADesc* mdesc, const std::string& type)
  {
    const std::string& nm = mdesc->nameBase();
    size_t pos = nm.find(type);
    return ((pos != std::string::npos) && (pos + type.length() == nm.length()));
  }


  static void
  convertToWorkMetric(Prof::Metric::ADesc* mdesc);

};


//***************************************************************************

class MPIBlameShiftIdlenessFact : public MetricComponentsFact
{
public:
  MPIBlameShiftIdlenessFact() { }
  virtual ~MPIBlameShiftIdlenessFact() { }

  virtual void
  make(Prof::CallPath::Profile& prof);
  
  virtual bool
  isSeparable(const Prof::CCT::AProcNode* x);

private:
  // make imbalance and idleness metrics
  void
  makeMetrics(Prof::CCT::ANode* node,
	      const std::vector<uint>& m_src,
	      const std::vector<uint>& m_imbalIncl,
	      const std::vector<uint>& m_imbalExcl,
	      const std::vector<uint>& m_idleIncl,
	      uint mId_bal,
	      Prof::Metric::AExprIncr* balancedExpr,
	      double balancedThreshold,
	      Prof::CCT::ANode* balancedFrm,
	      Prof::CCT::ANode* balancedNode);
  
private:
  static const std::string s_tag1;
  static const std::string s_tag2;
  static const std::string s_tag3;

};


//***************************************************************************

class PthreadOverheadMetricFact : public MetricComponentsFact
{
public:
  PthreadOverheadMetricFact() { }
  virtual ~PthreadOverheadMetricFact() { }
  
  virtual bool
  isSeparable(const Prof::CCT::AProcNode* x);

private:
  static const std::string s_tag;

};


//***************************************************************************

class CilkOverheadMetricFact : public MetricComponentsFact
{
public:
  CilkOverheadMetricFact() { }
  virtual ~CilkOverheadMetricFact() { }
  
  virtual bool
  isSeparable(const Prof::CCT::AProcNode* x);

private:
  static const std::string s_tag;

};


//***************************************************************************

} // namespace CallPath

} // namespace Analysis

//****************************************************************************

#endif // Analysis_CallPath_MetricComponentsFact_hpp
