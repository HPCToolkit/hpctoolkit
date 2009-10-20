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

#ifndef prof_juicy_MetricDesc 
#define prof_juicy_MetricDesc

//************************* System Include Files ****************************//

#include <iostream>
#include <vector>

//*************************** User Include Files ****************************//

#include <include/uint.h>

#include "Metric-AExpr.hpp"
#include "Metric-AExprItrv.hpp"

#include <lib/prof-lean/hpcrun-fmt.h>

//*************************** Forward Declarations **************************//

namespace Prof {

namespace Metric {

//***************************************************************************//
// ADesc
//***************************************************************************//

class ADesc
{
public:
  ADesc()
    : m_id(0),
      m_isVisible(true), m_isSortKey(false),
      m_doDispPercent(true), m_isPercent(false),
      m_isComputed(false)
  { }

  ADesc(const char* nameBase, const char* description,
	bool isVisible = true, bool isSortKey = false,
	bool doDispPercent = true, bool isPercent = false)
    : m_id(0), m_nameBase((nameBase) ? nameBase : ""), 
      m_description((description) ? description : ""),
      m_isVisible(isVisible), m_isSortKey(isSortKey),
      m_doDispPercent(doDispPercent), m_isPercent(isPercent),
      m_isComputed(false)
  { }

  ADesc(const std::string& nameBase, const std::string& description,
	bool isVisible = true, bool isSortKey = false,
	bool doDispPercent = true, bool isPercent = false)
    : m_id(0), m_nameBase(nameBase), m_description(description),
      m_isVisible(isVisible), m_isSortKey(isSortKey),
      m_doDispPercent(doDispPercent), m_isPercent(isPercent),
      m_isComputed(false)
  { }

  virtual ~ADesc()
  { }

  ADesc(const ADesc& x)
    : m_id(x.m_id), 
      m_nameBase(x.m_nameBase), m_namePfx(x.m_namePfx), m_nameSfx(x.m_nameSfx),
      m_description(x.m_description),
      m_isVisible(x.m_isVisible), m_isSortKey(x.m_isSortKey),
      m_doDispPercent(x.m_doDispPercent), m_isPercent(x.m_isPercent),
      m_isComputed(x.m_isComputed)
  { }

  ADesc&
  operator=(const ADesc& x) 
  {
    if (this != &x) {
      m_id            = x.m_id;
      m_nameBase      = x.m_nameBase;
      m_namePfx       = x.m_namePfx;
      m_nameSfx       = x.m_nameSfx;
      m_description   = x.m_description;
      m_isVisible     = x.m_isVisible;
      m_isSortKey     = x.m_isSortKey;
      m_doDispPercent = x.m_doDispPercent;
      m_isPercent     = x.m_isPercent;
      m_isComputed    = x.m_isComputed;
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
  // name: <prefix> <base> <suffix>
  // -------------------------------------------------------

  static const char nameSep = '.';

  const std::string
  name() const
  {
    // acceptable to create on demand
    std::string nm;
    if (!m_namePfx.empty()) { nm += m_namePfx + nameSep; }
    nm += m_nameBase;
    if (!m_nameSfx.empty()) { nm += nameSep + m_nameSfx; }
    return nm;
  }


  const std::string
  namePfxBase() const
  {
    // acceptable to create on demand
    std::string nm;
    if (!m_namePfx.empty()) { nm += m_namePfx + nameSep; }
    nm += m_nameBase;
    return nm;
  }


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
  { return m_isVisible; }

  void
  isVisible(bool x)
  { m_isVisible = x; }


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


  // has the metric been fully computed (e.g., have value been
  // aggregated from leaves to interior nodes) [we may want to split
  // this into isAggregated and isComputed]
  bool
  isComputed() const
  { return m_isComputed; }

  void
  isComputed(bool x)
  { m_isComputed = x; }


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  virtual std::string
  toString() const;

  std::ostream&
  dump(std::ostream& os = std::cerr) const
  {
    dump_me(os);
    return os;
  }

  virtual std::ostream&
  dump_me(std::ostream& os = std::cerr) const;

  void
  ddump() const;

protected:
private:
  uint m_id;

  std::string m_nameBase, m_namePfx, m_nameSfx;

  std::string m_description;

  bool m_isVisible;
  bool m_isSortKey;
  bool m_doDispPercent;
  bool m_isPercent;
  bool m_isComputed;
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
      m_period(0), m_flags(HPCRUN_MetricFlag_NULL), m_isUnitsEvents(false)
  { }

  SampledDesc(const char* nameBase, const char* description,
	      uint64_t period, bool isUnitsEvents,
	      const char* profName, const char* profRelId,
	      const char* profType,
	      bool isVisible = true, bool isSortKey = false,
	      bool doDispPercent = true, bool isPercent = false)
    : ADesc(nameBase, description,
	    isVisible, isSortKey, doDispPercent, isPercent),
      m_period(period), m_flags(HPCRUN_MetricFlag_NULL),
      m_isUnitsEvents(isUnitsEvents),
      m_profName(profName), m_profileRelId(profRelId), m_profileType(profType)
  { }

  SampledDesc(const std::string& nameBase, const std::string& description,
	      uint64_t period, bool isUnitsEvents,
	      const std::string& profName, const std::string& profRelId,
	      const std::string& profType, 
	      bool isVisible = true, bool isSortKey = false,
	      bool doDispPercent = true, bool isPercent = false)
    : ADesc(nameBase, description,
	    isVisible, isSortKey, doDispPercent, isPercent),
      m_period(period), m_flags(HPCRUN_MetricFlag_NULL),
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


  // profile-relative-id: metric id within associated profile file
  // ('select' attribute in HPCPROF config file)
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

  virtual std::ostream&
  dump_me(std::ostream& os = std::cerr) const;

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
	      bool isVisible = true, bool isSortKey = false,
	      bool doDispPercent = true, bool isPercent = false)
    : ADesc(nameBase, description,
	    isVisible, isSortKey, doDispPercent, isPercent),
      m_expr(expr)
  { }

  DerivedDesc(const std::string& nameBase, const std::string& description,
	      Metric::AExpr* expr,
	      bool isVisible = true, bool isSortKey = false,
	      bool doDispPercent = true, bool isPercent = false)
    : ADesc(nameBase, description,
	    isVisible, isSortKey, doDispPercent, isPercent),
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


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  virtual std::string
  toString() const;

  virtual std::ostream&
  dump_me(std::ostream& os = std::cerr) const;

protected:
private:
  Prof::Metric::AExpr* m_expr;
};


//***************************************************************************//
// DerivedItrvDesc
//***************************************************************************//

class DerivedItrvDesc : public ADesc
{
public:
  // Constructor: assumes ownership of 'expr'
  DerivedItrvDesc(const char* nameBase, const char* description,
		  Metric::AExprItrv* expr,
		  bool isVisible = true, bool isSortKey = false,
		  bool doDispPercent = true, bool isPercent = false)
    : ADesc(nameBase, description,
	    isVisible, isSortKey, doDispPercent, isPercent),
      m_expr(expr)
  { }

  DerivedItrvDesc(const std::string& nameBase, const std::string& description,
	      Metric::AExprItrv* expr,
	      bool isVisible = true, bool isSortKey = false,
	      bool doDispPercent = true, bool isPercent = false)
    : ADesc(nameBase, description,
	    isVisible, isSortKey, doDispPercent, isPercent),
      m_expr(expr)
  { }

  virtual ~DerivedItrvDesc()
  { delete m_expr; }
  
  DerivedItrvDesc(const DerivedItrvDesc& x)
    : ADesc(x),
      m_expr(x.m_expr)
  { DIAG_Die(DIAG_Unimplemented << "must copy expr!"); }
  
  DerivedItrvDesc&
  operator=(const DerivedItrvDesc& x) 
  {
    if (this != &x) {
      ADesc::operator=(x);
      m_expr = x.m_expr;
      DIAG_Die(DIAG_Unimplemented << "must copy expr!");
    }
    return *this;
  }

  virtual DerivedItrvDesc*
  clone() const
  { return new DerivedItrvDesc(*this); }


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  // N.B.: expr may be NULL if this metric is used as a helper
  Metric::AExprItrv*
  expr() const
  { return m_expr; }


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  virtual std::string
  toString() const;

  virtual std::ostream&
  dump_me(std::ostream& os = std::cerr) const;

protected:
private:
  Prof::Metric::AExprItrv* m_expr;
};


} // namespace Metric

} // namespace Prof

//***************************************************************************

#endif /* prof_juicy_ADesc */
