// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

#ifndef prof_juicy_Prof_Metric_ADesc_hpp 
#define prof_juicy_Prof_Metric_ADesc_hpp 

//************************ System Include Files ******************************

#include <string>

//************************* User Include Files *******************************

#include <include/uint.h>

#include "Metric-ADesc.hpp"
#include "Metric-AExpr.hpp"

#include <lib/support/diagnostics.h>

//************************ Forward Declarations ******************************

//****************************************************************************
// *** OBSOLETE ***
//****************************************************************************

//****************************************************************************

//namespace Prof {

//namespace Metric {


//****************************************************************************
//
//****************************************************************************

class PerfMetric {
public:
  
  PerfMetric(const char* name, bool isVisible, bool dispPercent,
	     bool isPercent, bool isSortKey)
    : m_id(0), m_name(name),
      m_isVisible(isVisible), m_isSortKey(isSortKey),
      m_dispPercent(dispPercent), m_isPercent(isPercent)
  { }
  
  PerfMetric(const std::string& name, bool isVisible, bool dispPercent,
	     bool isPercent, bool isSortKey)
    : m_id(0), m_name(name),
      m_isVisible(isVisible), m_isSortKey(isSortKey),
      m_dispPercent(dispPercent), m_isPercent(isPercent)
  { }
  
  virtual ~PerfMetric() { }

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  // id: index within Metric-Mgr
  uint id() const   { return m_id; }
  void id(uint id)  { m_id = id; }

  const std::string& name() const { return m_name; }
  void               name(const std::string& name) { m_name = name; }

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  // isVisible
  bool isVisible() const                { return m_isVisible; }
  void isVisible(bool isVisible)        { m_isVisible = isVisible; }

  bool isSortKey() const                { return m_isSortKey; }
  void isSortKey(bool isSortKey)        { m_isSortKey = isSortKey; }

  // display as a percentage
  bool dispPercent() const              { return m_dispPercent; }

  // Metric values should be interpreted as percents.  This is
  // important only if the value is to converted to be displayed as a
  // percent.  It is usually only relavent for derived metrics.
  bool isPercent() const                 { return m_isPercent; }

  virtual std::string toString(int flags = 0) const;


private: 
  uint m_id;
  std::string m_name;

  bool m_isVisible;
  bool m_isSortKey;
  bool m_dispPercent;
  bool m_isPercent;
};


//****************************************************************************
//
//****************************************************************************

class FilePerfMetric : public PerfMetric {
public: 
  FilePerfMetric(const char* name, 
		 bool isVisible, bool dispPercent, bool isSortKey, 
		 const char* profName, const char* nativeNm, const char* ftype,
		 bool isunit_ev)
    : PerfMetric(name, isVisible, dispPercent, false, isSortKey),
      m_profName(profName), m_nativeName(nativeNm), m_type(ftype),
      m_isUnitEvent(isunit_ev)
  { }

  FilePerfMetric(const std::string& name, 
		 bool isVisible, bool dispPercent, bool isSortKey, 
		 const std::string& profName, const std::string& nativeNm,
		 const std::string& ftype, 
		 bool isunit_ev)
    : PerfMetric(name, isVisible, dispPercent, false, isSortKey),
      m_profName(profName), m_nativeName(nativeNm), m_type(ftype),
      m_isUnitEvent(isunit_ev)
  { }
  
  virtual ~FilePerfMetric()
  { }

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  // A chinsy way of indicating the metric units of events rather than samples
  bool
  isUnitEvent() const
  { return m_isUnitEvent; }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  // associated profile filename (if available)
  const std::string&
  profileName() const
  { return m_profName; }

  // profile-relative-id: metric id within associated profile file
  // ('select' attribute in HPCPROF config file)
  const std::string&
  profileRelId() const
  { return m_nativeName; }

  // most likely obsolete
  const std::string&
  profileType() const
  { return m_type; } // HPCRUN, PROFILE


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  const Prof::Metric::SampledDesc& 
  rawdesc() const 
  { return m_rawdesc; }

  void 
  rawdesc(const Prof::Metric::SampledDesc& rawdesc) 
  { m_rawdesc = rawdesc; }
  

  virtual std::string toString(int flags = 0) const;

private: 
  std::string m_profName;
  std::string m_nativeName;
  std::string m_type;
  bool m_isUnitEvent;
  
  Prof::Metric::SampledDesc m_rawdesc;
};


//****************************************************************************
//
//****************************************************************************

class ComputedPerfMetric : public PerfMetric {
public: 
  ComputedPerfMetric(const char* name, bool isVisible, bool dispPercent, 
		     bool isPercent, bool isSortKey,
		     Prof::Metric::AExpr* expr)
    : PerfMetric(name, isVisible, dispPercent, isPercent, isSortKey),
      m_exprTree(expr)
  { }

  ComputedPerfMetric(const std::string& name, bool isVisible, bool dispPercent,
		     bool isPercent, bool isSortKey,
		     Prof::Metric::AExpr* expr)
    : PerfMetric(name, isVisible, dispPercent, isPercent, isSortKey),
      m_exprTree(expr)
  { }

  virtual ~ComputedPerfMetric()
  { delete m_exprTree; }

  const Prof::Metric::AExpr*
  expr() const
  { return m_exprTree; }

#if 0
  // FIXME:tallent This only applies to computed metrics.  It appears
  // to refer to a metric where the EvalTree is applied at the leaves
  // and then those results are accumulated to the interior nodes.
  // This seems useless and only a special case of the current functionality.
  bool PropComputed() const           { return pcomputed; }
#endif

  virtual std::string toString(int flags = 0) const;

private: 
  Prof::Metric::AExpr* m_exprTree; 
};


//****************************************************************************

//} // namespace Metric

//} // namespace Prof

//****************************************************************************

#endif // prof_juicy_Prof_Metric_ADesc_hpp
