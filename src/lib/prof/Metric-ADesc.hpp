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
// Copyright ((c)) 2002-2020, Rice University
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

#ifndef prof_MetricDesc 
#define prof_MetricDesc

//************************* System Include Files ****************************//

#include <iostream>
#include <vector>

//*************************** User Include Files ****************************//

#include <include/uint.h>

#include "Metric-AExpr.hpp"
#include "Metric-AExprIncr.hpp"

#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/support/diagnostics.h>

//*************************** macros or constants **************************//

#define ORDER_ARTIFICIAL_METRIC -1


//*************************** Forward Declarations **************************//

namespace Prof {

namespace Metric {

/*
 * type of sampling: period, frequency
 */
typedef enum SamplingType_e {
  PERIOD, FREQUENCY
} SamplingType_t;


//***************************************************************************//
// ADesc
//***************************************************************************//

class ADesc
{
public:
  static const uint id_NULL = UINT_MAX;

  // default value of formula: empty ? 
  // or do we want metric to itself (like default value of combine and finalize)? tbd.
  static const std::string s_formulaNULL;

public:
  ADesc()
    : m_id(id_NULL), m_type(TyNULL), m_partner(NULL), 
      m_visibility(HPCRUN_FMT_METRIC_SHOW), m_isSortKey(false),
      m_doDispPercent(true), m_isPercent(false),
      m_computedTy(ComputedTy_NULL),
      m_dbId(id_NULL), m_dbNumMetrics(0),
      m_num_samples(0), m_isMultiplexed(false),
      m_period_mean(0), m_sampling_type(FREQUENCY),
	  m_isTemporary(false), m_order(ORDER_ARTIFICIAL_METRIC)
  { }

  ADesc(const char* nameBase, const char* description,
	int visibility = HPCRUN_FMT_METRIC_SHOW, bool isSortKey = false,
	bool doDispPercent = true, bool isPercent = false)
    : m_id(id_NULL), m_type(TyNULL), m_partner(NULL),
      m_description((description) ? description : ""),
      m_visibility(visibility), m_isSortKey(isSortKey),
      m_doDispPercent(doDispPercent), m_isPercent(isPercent),
      m_computedTy(ComputedTy_NULL),
      m_dbId(id_NULL), m_dbNumMetrics(0),
      m_num_samples(0), m_isMultiplexed(false),
      m_period_mean(0), m_sampling_type(FREQUENCY),
	  m_isTemporary(false), m_order(ORDER_ARTIFICIAL_METRIC)
  {
    std::string nm = (nameBase) ? nameBase : "";
    nameFromString(nm);
  }

  ADesc(const std::string& nameBase, const std::string& description,
	int visibility = HPCRUN_FMT_METRIC_SHOW, bool isSortKey = false,
	bool doDispPercent = true, bool isPercent = false)
    : m_id(id_NULL), m_type(TyNULL), m_partner(NULL),
      m_description(description),
      m_visibility(visibility), m_isSortKey(isSortKey),
      m_doDispPercent(doDispPercent), m_isPercent(isPercent),
      m_computedTy(ComputedTy_NULL),
      m_dbId(id_NULL), m_dbNumMetrics(0),
      m_num_samples(0), m_isMultiplexed(false),
      m_period_mean(0), m_sampling_type(FREQUENCY),
	  m_isTemporary(false), m_order(ORDER_ARTIFICIAL_METRIC)
  {
    nameFromString(nameBase);
  }

  virtual ~ADesc()
  { }

  ADesc(const ADesc& x)
    : m_id(x.m_id), m_type(x.m_type), m_partner(x.m_partner),
      m_nameBase(x.m_nameBase), m_namePfx(x.m_namePfx), m_nameSfx(x.m_nameSfx),
      m_description(x.m_description),
      m_visibility(x.m_visibility), m_isSortKey(x.m_isSortKey),
      m_doDispPercent(x.m_doDispPercent), m_isPercent(x.m_isPercent),
      m_computedTy(x.m_computedTy),
      m_dbId(x.m_dbId), m_dbNumMetrics(x.m_dbNumMetrics),
      m_num_samples(x.m_num_samples), m_isMultiplexed(x.m_isMultiplexed),
      m_period_mean(x.m_period_mean), m_sampling_type(x.m_sampling_type),
      m_isTemporary(false), m_formula(x.m_formula), m_format(x.m_format), 
      m_order(x.m_order)
  { }

  ADesc&
  operator=(const ADesc& x) 
  {
    if (this != &x) {
      m_id            = x.m_id;
      m_type          = x.m_type;
      m_partner       = x.m_partner;
      m_nameBase      = x.m_nameBase;
      m_namePfx       = x.m_namePfx;
      m_nameSfx       = x.m_nameSfx;
      m_description   = x.m_description;
      m_visibility    = x.m_visibility;
      m_isSortKey     = x.m_isSortKey;
      m_doDispPercent = x.m_doDispPercent;
      m_isPercent     = x.m_isPercent;
      m_computedTy    = x.m_computedTy;
      m_dbId          = x.m_dbId;
      m_dbNumMetrics  = x.m_dbNumMetrics;

      m_num_samples   = x.m_num_samples;
      m_isMultiplexed = x.m_isMultiplexed;
      m_period_mean   = x.m_period_mean;
      m_sampling_type = x.m_sampling_type;

      m_isTemporary   = x.m_isTemporary;
      m_formula       = x.m_formula;
      m_format        = x.m_format;
      m_order         = x.m_order;
    }
    return *this;
  }

  virtual ADesc*
  clone() const
  { return new ADesc(*this); }


  // -------------------------------------------------------
  // id:
  // -------------------------------------------------------

  uint
  id() const
  { return m_id; }

  void
  id(uint id)
  { m_id = id; }

  // -------------------------------------------------------
  // hpcrun metric sequence order: need to keep the sequence order
  // since hpcprof will reorder the ID.
  // The sequence order is important when hpcrun has its
  // metric formula to be passed to hpcviewer (or computed by hpcprof?)
  // -------------------------------------------------------

  void 
  order(int sequence)
  { m_order = sequence; }

  int
  order() const
  {return m_order;} 

  // -------------------------------------------------------
  // type:
  // -------------------------------------------------------

  enum ADescTy {
    TyNULL = 0,
    TyIncl,
    TyExcl
  };


  static const std::string s_nameNULL;
  static const std::string s_nameIncl;
  static const std::string s_nameExcl;

  static const std::string&
  ADescTyToString(ADescTy type);

  static const char*
  ADescTyToXMLString(ADescTy type);

  static ADescTy
  stringToADescTy(const std::string& x);


  ADescTy
  type() const
  { return m_type; }

  void
  type(ADescTy type)
  { m_type = type; }


  // -------------------------------------------------------
  // partner: inclusive/exclusive metrics come in pairs
  // -------------------------------------------------------

  ADesc*
  partner() const
  { return m_partner; }

  void
  partner(ADesc* partner)
  { m_partner = partner; }


  // -------------------------------------------------------
  // name: <prefix> <base> <suffix>
  // -------------------------------------------------------

  static const char nameSep = '.';

  static const std::string s_nameFmtTag;
  static const char s_nameFmtSegBeg = '{'; // shouldn't need to escape
  static const char s_nameFmtSegEnd = '}';


  std::string
  name() const
  {
    // acceptable to create on demand
    std::string nm = namePfxBaseSfx();
    nm += ADescTyToString(type());
    return nm;
  }


  // nameGeneric: a 'generic' name for the metric (i.e., without the
  // suffix qualifier)
  std::string
  nameGeneric() const
  {
    std::string nm = namePfxBase();
    nm += ADescTyToString(type());
    return nm;
  }

  std::string
  namePfxBase() const
  {
    // acceptable to create on demand
    std::string nm;
    if (!m_namePfx.empty()) { nm += m_namePfx + nameSep; }
    nm += m_nameBase;
    return nm;
  }

  std::string
  namePfxBaseSfx() const
  {
    // acceptable to create on demand
    std::string nm = namePfxBase();
    if (!m_nameSfx.empty()) { nm += nameSep + m_nameSfx; }
    return nm;
  }


  // nameToFmt: generate formatted string
  std::string
  nameToFmt() const;

  // nameFromString: if 'x' is a formatted string, set the various name
  // components; otherwise set base = x.
  void
  nameFromString(const std::string& x);


  const std::string&
  nameBase() const
  { return m_nameBase; }

  void
  nameBase(const char* x)
  { m_nameBase = (x) ? x : ""; }

  void
  nameBase(const std::string& x)
  { m_nameBase = x; }


  const std::string&
  namePfx() const
  { return m_namePfx; }

  void
  namePfx(const char* x)
  { m_namePfx = (x) ? x : ""; }

  void
  namePfx(const std::string& x)
  { m_namePfx = x; }


  const std::string&
  nameSfx() const
  { return m_nameSfx; }

  void
  nameSfx(const char* x)
  { m_nameSfx = (x) ? x : ""; }

  void
  nameSfx(const std::string& x)
  { m_nameSfx = x; }

  // -------------------------------------------------------
  // description:
  // -------------------------------------------------------

  const std::string&
  description() const
  { return m_description; }

  void
  description(const char* x)
  { m_description = (x) ? x : ""; }

  void
  description(const std::string& x)
  { m_description = x; }


  // ------------------------------------------------------------
  // useful attributes
  // ------------------------------------------------------------

  // isVisible
  bool
  isVisible() const
  { return m_visibility != HPCRUN_FMT_METRIC_HIDE; }


  int
  visibility() const
  { return m_visibility; }

  void
  visibility(int x)
  { m_visibility = x; }


  bool
  isSortKey() const
  { return m_isSortKey; }

  void
  isSortKey(bool x)
  { m_isSortKey = x; }


  // display as a percentage
  bool
  doDispPercent() const
  { return m_doDispPercent; }

  void
  doDispPercent(bool x)
  { m_doDispPercent = x; }


  // Metric values should be interpreted as percents.  This is
  // important only if the value is to converted to be displayed as a
  // percent.  It is usually only relavent for derived metrics.
  //
  // Note: This is a little clumsy.  Possibly it should be
  // incorporated into a "units" attribute.  But this will have to do
  // for now.
  bool
  isPercent() const
  { return m_isPercent; }


  bool
  isTemporary() const
  { return m_isTemporary; }

  void
  isTemporary(bool x)
  { m_isTemporary = x; }

  // ------------------------------------------------------------
  // computed type
  // ------------------------------------------------------------

  enum ComputedTy {
    ComputedTy_NULL = 0, // no aggregegation from leaves to interior nodes
    ComputedTy_NonFinal, // non-finalized values
    ComputedTy_Final     // finalized values
  };

  ComputedTy
  computedType() const
  { return m_computedTy; }

  void
  computedType(ComputedTy x)
  { m_computedTy = x; }


  // -------------------------------------------------------
  // metric DB info
  // -------------------------------------------------------

  bool
  hasDBInfo() const
  { return (m_dbId != id_NULL && m_dbNumMetrics > 0); }

  void
  zeroDBInfo()
  {
    dbId(id_NULL);
    dbNumMetrics(0);
  }


  const std::string
  dbFileGlob() const
  {
    std::string dbFileGlob;
    if (!m_namePfx.empty()) { dbFileGlob += m_namePfx + nameSep; }
    dbFileGlob += std::string("*.") + HPCPROF_MetricDBSfx;
    return dbFileGlob;
  }


  uint
  dbId() const
  { return m_dbId; }

  void
  dbId(uint x)
  { m_dbId = x; }


  uint
  dbNumMetrics() const
  { return m_dbNumMetrics; }

  void
  dbNumMetrics(uint x)
  { m_dbNumMetrics = x; }


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  virtual std::string
  toString() const;

  virtual std::string
  toValueTyStringXML() const
  { DIAG_Die(DIAG_Unimplemented); return ""; }

  std::ostream&
  dump(std::ostream& os = std::cerr) const
  {
    dumpMe(os);
    return os;
  }

  virtual std::ostream&
  dumpMe(std::ostream& os = std::cerr) const;

  void
  ddump() const;


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  static MetricFlags_ValTy_t
  toHPCRunMetricValTy(ADescTy ty);

  static ADescTy
  fromHPCRunMetricValTy(MetricFlags_ValTy_t ty);

  void 
  formula(std::string formula)
  { m_formula = formula; }

  std::string 
  formula() const
  { return m_formula; }

  void 
  format(std::string fmt)
  { m_format = fmt; }

  std::string
  format() const
  { return m_format; }

  // -------------------------------------------------------
  // perf-event additional info
  // -------------------------------------------------------

  void
  isMultiplexed(bool isMultiplexedEvent)
  { m_isMultiplexed = isMultiplexedEvent; }

  bool
  isMultiplexed() const
  { return m_isMultiplexed; }

  void
  periodMean(float periodMeanEvent)
  { m_period_mean = periodMeanEvent; }

  float
  periodMean() const
  {return m_period_mean; }

  void
  sampling_type(SamplingType_t type)
  { m_sampling_type = type; }

  SamplingType_t
  sampling_type() const
  { return m_sampling_type; }

  void
  num_samples(const uint64_t samples)
  { m_num_samples = samples; }

  uint64_t
  num_samples() const
  { return m_num_samples; }

protected:
private:
  uint m_id;
  ADescTy m_type;
  ADesc* m_partner;

  std::string m_nameBase, m_namePfx, m_nameSfx;
  std::string m_description;

  int  m_visibility;
  bool m_isSortKey;
  bool m_doDispPercent;
  bool m_isPercent;

  ComputedTy m_computedTy;

  uint m_dbId;
  uint m_dbNumMetrics;

  // perf event additional attributes
  uint64_t m_num_samples;
  bool  m_isMultiplexed;
  float m_period_mean;
  enum SamplingType_e m_sampling_type;

  bool m_isTemporary;

  // hpcrun metric formula
  std::string m_formula;

  // display format                                  
  std::string m_format;

  // hpcrun metric order
  int m_order;
};


//***************************************************************************//
// ADescVec
//***************************************************************************//

typedef std::vector<ADesc*> ADescVec;

//class ADescVec : public std::vector<ADesc*> { };


//***************************************************************************//
// SampledDesc
//***************************************************************************//

class SampledDesc : public ADesc
{
public:
  SampledDesc()
    : ADesc(),
      m_period(0), m_flags(hpcrun_metricFlags_NULL), m_isUnitsEvents(false)
  { }

  SampledDesc(const char* nameBase, const char* description,
	      uint64_t period, bool isUnitsEvents,
	      const char* profName, const char* profRelId,
	      const char* profType,
	      int visibility = HPCRUN_FMT_METRIC_SHOW, bool isSortKey = false,
	      bool doDispPercent = true, bool isPercent = false)
    : ADesc(nameBase, description,
	    visibility, isSortKey, doDispPercent, isPercent),
      m_period(period), m_flags(hpcrun_metricFlags_NULL),
      m_isUnitsEvents(isUnitsEvents),
      m_profName(profName), m_profileRelId(profRelId), m_profileType(profType)
  { }

  SampledDesc(const std::string& nameBase, const std::string& description,
	      uint64_t period, bool isUnitsEvents,
	      const std::string& profName, const std::string& profRelId,
	      const std::string& profType, 
	      int visibility = HPCRUN_FMT_METRIC_SHOW, bool isSortKey = false,
	      bool doDispPercent = true, bool isPercent = false)
    : ADesc(nameBase, description,
	    visibility, isSortKey, doDispPercent, isPercent),
      m_period(period), m_flags(hpcrun_metricFlags_NULL),
      m_isUnitsEvents(isUnitsEvents),
      m_profName(profName), m_profileRelId(profRelId), m_profileType(profType)
  { }

  virtual ~SampledDesc()
  { }
  
  SampledDesc(const SampledDesc& x)
    : ADesc(x),
      m_period(x.m_period), m_flags(x.m_flags),
      m_isUnitsEvents(x.m_isUnitsEvents),
      m_profName(x.m_profName), m_profileRelId(x.m_profileRelId),
      m_profileType(x.m_profileType)
  { }
  
  SampledDesc&
  operator=(const SampledDesc& x) 
  {
    if (this != &x) {
      ADesc::operator=(x);
      m_period = x.m_period;
      m_flags  = x.m_flags;
      m_isUnitsEvents = x.m_isUnitsEvents;
      m_profName      = x.m_profName;
      m_profileRelId  = x.m_profileRelId;
      m_profileType   = x.m_profileType;
    }
    return *this;
  }

  virtual SampledDesc*
  clone() const
  { return new SampledDesc(*this); }


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  uint64_t 
  period() const
  { return m_period; }
  
  void
  period(uint64_t x)
  { m_period = x; }


  hpcrun_metricFlags_t
  flags() const
  { return m_flags; }
  
  void
  flags(hpcrun_metricFlags_t x) 
  { m_flags = x; }


  // A chinsy way of indicating the metric units of events rather than
  // samples or something else: FIXME: move to base class
  bool
  isUnitsEvents() const
  { return m_isUnitsEvents; }

  void
  isUnitsEvents(bool isUnitsEvents)
  { m_isUnitsEvents = isUnitsEvents; }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  // associated profile filename (if available)
  const std::string&
  profileName() const
  { return m_profName; }

  void
  profileName(std::string profName)
  { m_profName = profName; }


  // -------------------------------------------------------
  // profile-relative-id: metric id within associated profile file
  // ('select' attribute in HPCPROF config file)
  // -------------------------------------------------------
  const std::string&
  profileRelId() const
  { return m_profileRelId; }

  void
  profileRelId(std::string profileRelId)
  { m_profileRelId = profileRelId; }


  // most likely obsolete: HPCRUN, PROFILE
  const std::string&
  profileType() const
  { return m_profileType; } 

  void
  profileType(std::string profileType)
  { m_profileType = profileType; }


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  virtual std::string
  toString() const;

  virtual std::string
  toValueTyStringXML() const;

  virtual std::ostream&
  dumpMe(std::ostream& os = std::cerr) const;


protected:
private:
  uint64_t m_period;             // sampling period
  hpcrun_metricFlags_t m_flags;  // flags of the metric
  bool m_isUnitsEvents;

  std::string m_profName;
  std::string m_profileRelId;
  std::string m_profileType;
};


//***************************************************************************//
// SampledDescVec
//***************************************************************************//

class SampledDescVec : public std::vector<SampledDesc*>
{
};


//***************************************************************************//
// DerivedDesc
//***************************************************************************//

class DerivedDesc : public ADesc
{
public:
  // Constructor: assumes ownership of 'expr'
  DerivedDesc(const char* nameBase, const char* description,
	      Metric::AExpr* expr,
	      int visibility = HPCRUN_FMT_METRIC_SHOW, bool isSortKey = false,
	      bool doDispPercent = true, bool isPercent = false)
    : ADesc(nameBase, description,
	    visibility, isSortKey, doDispPercent, isPercent),
      m_expr(expr)
  { }

  DerivedDesc(const std::string& nameBase, const std::string& description,
	      Metric::AExpr* expr,
	      int visibility = HPCRUN_FMT_METRIC_SHOW, bool isSortKey = false,
	      bool doDispPercent = true, bool isPercent = false)
    : ADesc(nameBase, description,
	    visibility, isSortKey, doDispPercent, isPercent),
      m_expr(expr)
  { }

  virtual ~DerivedDesc()
  { delete m_expr; }
  
  DerivedDesc(const DerivedDesc& x)
    : ADesc(x),
      m_expr(x.m_expr)
  { DIAG_Die(DIAG_Unimplemented << "must copy expr!"); }
  
  DerivedDesc&
  operator=(const DerivedDesc& x) 
  {
    if (this != &x) {
      ADesc::operator=(x);
      m_expr = x.m_expr;
      DIAG_Die(DIAG_Unimplemented << "must copy expr!");
    }
    return *this;
  }

  virtual DerivedDesc*
  clone() const
  { return new DerivedDesc(*this); }


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  Metric::AExpr*
  expr() const
  { return m_expr; }

  void
  expr(Metric::AExpr* x)
  { m_expr = x; /* assumes ownership of x */ }


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  virtual std::string
  toString() const;

  virtual std::string
  toValueTyStringXML() const;

  virtual std::ostream&
  dumpMe(std::ostream& os = std::cerr) const;

protected:
private:
  Prof::Metric::AExpr* m_expr;
};


//***************************************************************************//
// DerivedIncrDesc
//***************************************************************************//

class DerivedIncrDesc : public ADesc
{
public:
  // Constructor: assumes ownership of 'expr'
  DerivedIncrDesc(const char* nameBase, const char* description,
		  Metric::AExprIncr* expr,
		  int visibility = HPCRUN_FMT_METRIC_SHOW, bool isSortKey = false,
		  bool doDispPercent = true, bool isPercent = false)
    : ADesc(nameBase, description,
	    visibility, isSortKey, doDispPercent, isPercent),
      m_expr(expr)
  { }

  DerivedIncrDesc(const std::string& nameBase, const std::string& description,
	      Metric::AExprIncr* expr,
	      int visibility = HPCRUN_FMT_METRIC_SHOW, bool isSortKey = false,
	      bool doDispPercent = true, bool isPercent = false)
    : ADesc(nameBase, description,
	    visibility, isSortKey, doDispPercent, isPercent),
      m_expr(expr)
  { }

  virtual ~DerivedIncrDesc()
  { delete m_expr; }
  
  DerivedIncrDesc(const DerivedIncrDesc& x)
    : ADesc(x),
      m_expr(x.m_expr)
  { /* DIAG_Die(DIAG_Unimplemented << "must copy expr!"); */ m_expr = NULL; }
  
  DerivedIncrDesc&
  operator=(const DerivedIncrDesc& x) 
  {
    if (this != &x) {
      ADesc::operator=(x);
      m_expr = x.m_expr;
      DIAG_Die(DIAG_Unimplemented << "must copy expr!");
    }
    return *this;
  }

  virtual DerivedIncrDesc*
  clone() const
  { return new DerivedIncrDesc(*this); }


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  // N.B.: expr may be NULL if this metric is used as a helper
  Metric::AExprIncr*
  expr() const
  { return m_expr; }

  void
  expr(Metric::AExprIncr* x)
  { m_expr = x; /* assumes ownership of x */ }


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  virtual std::string
  toString() const;

  virtual std::string
  toValueTyStringXML() const;

  virtual std::ostream&
  dumpMe(std::ostream& os = std::cerr) const;

protected:
private:
  Prof::Metric::AExprIncr* m_expr;
};


} // namespace Metric

} // namespace Prof

//***************************************************************************

#endif /* prof_ADesc */
