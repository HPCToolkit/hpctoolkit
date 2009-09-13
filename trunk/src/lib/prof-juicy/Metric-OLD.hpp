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
  
  PerfMetric(const char* nm, const char* displayNm,
	     bool disp, bool dispPercent, bool isPercent, bool sort)
    : name(nm), m_dispName(displayNm), display(disp), 
      m_dispPercent(dispPercent), m_isPercent(isPercent), sortBy(sort), m_id(0)
  { }
  
  PerfMetric(const std::string& nm, const std::string& displayNm, 
	     bool disp, bool dispPercent, bool isPercent, bool sort)
    : name(nm), m_dispName(displayNm), display(disp), 
      m_dispPercent(dispPercent), m_isPercent(isPercent), sortBy(sort), m_id(0)
  { }
  
  virtual ~PerfMetric() { }

  const std::string& Name() const          { return name; }
  void               Name(const std::string& nm)  { name = nm; }

  const std::string& dispName() const          { return m_dispName; }
  void               dispName(const std::string& nm)  { m_dispName = nm; }

  // set by outside
  uint Index() const   { return m_id; }
  void Index(uint id)  { m_id = id; }
  
  bool Display() const                { return display; }
  void Display(bool display_)         { display = display_; }

  // display as a percentage
  bool dispPercent() const            { return m_dispPercent; }

  // value is already a percent (if not, it must be converted to
  // display as percentage).  This is especially critical for computed
  // metrics where dispPercent is ambiguous.
  bool isPercent() const              { return m_isPercent; }

  bool SortBy() const                 { return sortBy; }
  void SortBy(bool sortBy_)           { sortBy = sortBy_; }

  virtual std::string toString(int flags = 0) const;


private: 
  std::string name;
  std::string m_dispName;
  bool display; 

  bool m_dispPercent; 
  bool m_isPercent; 

  bool sortBy;
  
protected: 
  uint m_id;
};


//****************************************************************************
//
//****************************************************************************

class FilePerfMetric : public PerfMetric {
public: 
  FilePerfMetric(const char* nm, const char* displayNm,
		 bool display, bool dispPercent, bool sortBy, 
		 const char* fname, const char* nativeNm, const char* ftype,
		 bool isunit_ev)
    : PerfMetric(nm, displayNm, display, dispPercent, false, sortBy),
      m_file(fname), m_nativeName(nativeNm), m_type(ftype),
      m_isunit_event(isunit_ev)
  { }

  FilePerfMetric(const std::string& nm, const std::string& displayNm,
		 bool display, bool dispPercent, bool sortBy, 
		 const std::string& fname, const std::string& nativeNm,
		 const std::string& ftype, 
		 bool isunit_ev)
    : PerfMetric(nm, displayNm, display, dispPercent, false, sortBy),
      m_file(fname), m_nativeName(nativeNm), m_type(ftype),
      m_isunit_event(isunit_ev)
  { }
  
  virtual ~FilePerfMetric()
  { }

  const std::string&
  FileName() const
  { return m_file; }

  // metric id within profile file ('select' attribute in HPCPROF config file)
  const std::string&
  NativeName() const
  { return m_nativeName; }

  const std::string&
  FileType() const
  { return m_type; } // HPCRUN, PROFILE

  bool
  isunit_event() const
  { return m_isunit_event; }


  const Prof::Metric::SampledDesc& 
  rawdesc() const 
  { return m_rawdesc; }

  void 
  rawdesc(const Prof::Metric::SampledDesc& rawdesc) 
  { m_rawdesc = rawdesc; }
  

  virtual std::string toString(int flags = 0) const;

private: 
  std::string m_file;
  std::string m_nativeName;
  std::string m_type;
  bool m_isunit_event;
  
  Prof::Metric::SampledDesc m_rawdesc;
};


//****************************************************************************
//
//****************************************************************************

class ComputedPerfMetric : public PerfMetric {
public: 
  ComputedPerfMetric(const char* nm, const char* displayNm,
		     bool disp, bool dispPercent, bool isPercent, bool doSort, 
		     //bool propagateComputed, 
		     Prof::Metric::AExpr* expr)
    : PerfMetric(nm, displayNm, disp, dispPercent, isPercent, doSort),
      m_exprTree(expr)
  { }

  ComputedPerfMetric(const std::string& nm, const std::string& displayNm,
		     bool disp, bool dispPercent, bool isPercent, bool doSort, 
		     //bool propagateComputed, 
		     Prof::Metric::AExpr* expr)
    : PerfMetric(nm, displayNm, disp, dispPercent, isPercent, doSort),
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
