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


class DataDisplayInfo { 
public: 
  DataDisplayInfo(const char* nam, // should not be empty
		  const char* col, // may be NULL for undefined 
		  unsigned int w, 
                  bool formtAsInt)
    : name(nam), color((col ? col : "")), width(w), formatAsInt(formtAsInt)
    { }

  ~DataDisplayInfo();

  const std::string&  Name()        const { return name; }
  void                Name(const std::string& nm)  { name = nm; }
  const std::string&  Color()       const { return color; }
  unsigned int        Width()       const { return width; }
  bool                FormatAsInt() const { return formatAsInt; }
  
  std::string toString() const; 

private:
  void SetWidth(unsigned int w) { width = w; } 

  std::string name; 
  std::string color;  
  unsigned int width; 
  bool formatAsInt; 
}; 


//****************************************************************************
//
//****************************************************************************

class PerfMetric {
public:
  // eraxxon: Note on 'nativeName' argument: This argument doesn't
  // have a lot of meaning except with 'FilePerfMetric';
  // ComputedPerfMetrics aren't 'native'.  Really, it would be cleaner
  // to have a type field (but then this file would be dependent on
  // 'DerivedPerfMetrics') or use runtime type identification (which
  // most compilers support now).  For now, however, since the HPCVIEW
  // metric contains a native name attribute, this is reasonable (and
  // fastest).
  
  PerfMetric(const char *name, const char *nativeName, const char* displayName,
	     bool display, bool dispPercent, bool isPercent, bool sortBy);

  PerfMetric(const std::string& name, const std::string& nativeName, 
	     const std::string& displayName, 
	     bool display, bool dispPercent, bool isPercent, bool sortBy);

  // constructor automatically adds new instance to PerfMetricTable
  // and sets this->perfInfoIndex to instance's index in the table
  
  virtual ~PerfMetric();       

  const std::string& Name() const          { return name; }
  void               Name(const std::string& nm)  { name = nm; }
  const std::string& NativeName() const    { return nativeName; }
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

#if 0
  // FIXME:tallent This only applies to computed metrics.  It appears
  // to refer to a metric where the EvalTree is applied at the leaves
  // and then those results are accumulated to the interior nodes.
  // This seems useless and only a special case of the current functionality.
  bool PropComputed() const           { return pcomputed; }
#endif
  
  DataDisplayInfo& DisplayInfo() const { return *dispInfo; }; 
  
  // flags: default -> display output
  //        non-zero -> debugging output
  virtual std::string toString(int flags = 0) const; 

private:
  void Ctor(const char *name, const char* displayName); 

private: 
  std::string name;
  std::string nativeName;
  DataDisplayInfo *dispInfo; 
  bool display; 

  bool m_dispPercent; 
  bool m_isPercent; 

  bool pcomputed; 
  bool sortBy;
  
protected: 
  uint m_id;
};


//****************************************************************************
//
//****************************************************************************

class FilePerfMetric : public PerfMetric {
public: 
  // NOTE: NativeName() is the 'select' attribute
  FilePerfMetric(const char* nm, const char* nativeNm, const char* displayNm,
		 bool display, bool dispPercent, bool sortBy, 
		 const char* fname, const char* ftype,
		 bool isuint_ev); 
  FilePerfMetric(const std::string& nm, const std::string& nativeNm, 
		 const std::string& displayNm,
		 bool display, bool dispPercent, bool sortBy, 
		 const std::string& fname, const std::string& ftype, 
		 bool isunit_ev); 

  virtual ~FilePerfMetric(); 

  // --------------------------------------------------------
  
  const std::string& FileName() const { return m_file; }
  const std::string& FileType() const { return m_type; } // HPCRUN, PROFILE

  const Prof::Metric::SampledDesc& 
  rawdesc() const 
  { 
    return m_rawdesc; 
  }

  void 
  rawdesc(const Prof::Metric::SampledDesc& rawdesc) 
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
  
  Prof::Metric::SampledDesc m_rawdesc;
};


//****************************************************************************
//
//****************************************************************************

class ComputedPerfMetric : public PerfMetric {
public: 
  ComputedPerfMetric(const char* nm, const char* displayNm,
		     bool display, bool dispPercent, bool sortBy, 
		     bool propagateComputed, 
		     Prof::Metric::AExpr* expr);
  ComputedPerfMetric(const std::string& nm, const std::string& displayNm,
		     bool display, bool dispPercent, bool sortBy, 
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

//} // namespace Metric

//} // namespace Prof

//****************************************************************************

#endif // prof_juicy_Prof_Metric_ADesc_hpp
