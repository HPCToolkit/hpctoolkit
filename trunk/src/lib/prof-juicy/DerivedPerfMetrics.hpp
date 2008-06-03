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

#ifndef Prof_Metric_DerivedPerfMetrics_hpp 
#define Prof_Metric_DerivedPerfMetrics_hpp 

//************************ System Include Files ******************************

#include <string>

//************************* User Include Files *******************************

#include <include/general.h> 

#include "Metric-AExpr.hpp"
#include "MetricDesc.hpp"
#include "PerfMetric.hpp"


//************************ Forward Declarations ******************************

//****************************************************************************
//
//****************************************************************************

class FilePerfMetric : public PerfMetric {
public: 
  // NOTE: NativeName() is the 'select' attribute
  FilePerfMetric(const char* nm, const char* nativeNm, const char* displayNm,
		 bool display, bool percent, bool sortBy, 
		 const char* fname, const char* ftype,
		 bool isuint_ev); 
  FilePerfMetric(const std::string& nm, const std::string& nativeNm, 
		 const std::string& displayNm,
		 bool display, bool percent, bool sortBy, 
		 const std::string& fname, const std::string& ftype, 
		 bool isunit_ev); 

  virtual ~FilePerfMetric(); 

  // --------------------------------------------------------
  
  const std::string& FileName() const { return m_file; }
  const std::string& FileType() const { return m_type; } // HPCRUN, PROFILE

  const Prof::SampledMetricDesc& 
  rawdesc() const 
  { 
    return m_rawdesc; 
  }

  void 
  rawdesc(const Prof::SampledMetricDesc& rawdesc) 
  { 
    m_rawdesc = rawdesc; 
  }
  

  bool isunit_event() const { return m_isunit_event; }

  // --------------------------------------------------------


  virtual std::string toString(int flags = 0) const; 

private: 
  std::string m_file;
  std::string m_type; // for later use
  bool m_isunit_event;
  
  Prof::SampledMetricDesc m_rawdesc;
};


//****************************************************************************
//
//****************************************************************************

class ComputedPerfMetric : public PerfMetric {
public: 
  ComputedPerfMetric(const char* nm, const char* displayNm,
		     bool display, bool percent, bool sortBy, 
		     bool propagateComputed, 
		     Prof::Metric::AExpr* expr);
  ComputedPerfMetric(const std::string& nm, const std::string& displayNm,
		     bool display, bool percent, bool sortBy, 
		     bool propagateComputed, 
		     Prof::Metric::AExpr* expr);

  virtual ~ComputedPerfMetric(); 

  virtual std::string toString(int flags = 0) const; 

  const Prof::Metric::AExpr* expr() const { return m_exprTree; }

private:
  void Ctor(const char* nm, Prof::Metric::AExpr* expr);

private: 
  Prof::Metric::AExpr* m_exprTree; 
};

//****************************************************************************

#endif // prof_juicy_Prof_Metric_DerivedPerfMetrics_hpp
