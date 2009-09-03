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

#include <lib/prof-lean/hpcrun-fmt.h>

//*************************** Forward Declarations **************************//

namespace Prof {

//***************************************************************************//
// MetricDesc
//***************************************************************************//

class MetricDesc
{
public:
  MetricDesc() 
    { }

  MetricDesc(const char* name, const char* description)
    : m_name((name) ? name : ""), 
      m_description((description) ? description : "")
    { }

  MetricDesc(const std::string& name, const std::string& description)
    : m_name(name), m_description(description)
    { }

  virtual ~MetricDesc() { }

  MetricDesc(const MetricDesc& x)
    : m_name(x.m_name), m_description(x.m_description)
    { }

  MetricDesc& operator=(const MetricDesc& x) {
    if (this != &x) {
      m_name        = x.m_name;
      m_description = x.m_description;
    }
    return *this;
  }

  // -------------------------------------------------------
  // name, description: The metric name and a description
  // -------------------------------------------------------
  const std::string& name() const        { return m_name; }
  void               name(const char* x) { m_name = (x) ? x : ""; }
  void               name(const std::string& x) { m_name = x; }

  const std::string& description() const { return m_description; }
  void description(const char* x)        { m_description = (x) ? x : ""; }
  void description(const std::string& x) { m_description = x; }

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  void dump(std::ostream& os = std::cerr) const { }
  void ddump() const { }

protected:
private:  
  std::string m_name;
  std::string m_description;
};


//***************************************************************************//
// SampledMetricDesc
//***************************************************************************//

class SampledMetricDesc : public MetricDesc
{
public:
  SampledMetricDesc() 
    : m_flags(HPCRUN_MetricFlag_NULL), m_period(0) 
    { }

  SampledMetricDesc(const char* name, const char* description,
		    unsigned long period)
    : MetricDesc(name, description),
      m_flags(HPCRUN_MetricFlag_NULL), m_period(period)
    { }

  virtual ~SampledMetricDesc() 
    { }

  SampledMetricDesc(const SampledMetricDesc& x)
    : MetricDesc(x.name(), x.description()), 
      m_flags(x.m_flags), m_period(x.m_period)
    { }

  SampledMetricDesc& operator=(const SampledMetricDesc& x) {
    MetricDesc::operator=(x);
    if (this != &x) {
      m_flags = x.m_flags;      
      m_period = x.m_period;
    }
    return *this;
  }

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  hpcrun_metricFlags_t
  flags() const
  { return m_flags; }
  
  void
  flags(hpcrun_metricFlags_t x) 
  { m_flags = x; }

  uint64_t 
  period() const
  { return m_period; }
  
  void
  period(uint64_t x)
  { m_period = x; }

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  void dump(std::ostream& os = std::cerr) const { }
  void ddump() const { }

protected:
private:  
  hpcrun_metricFlags_t m_flags;  // flags of the metric
  uint64_t m_period; // sampling period
};


//***************************************************************************//
// SampledMetricDesc
//***************************************************************************//

class SampledMetricDescVec : public std::vector<SampledMetricDesc*>
{
};


} // namespace Prof

//***************************************************************************

#endif /* prof_juicy_MetricDesc */
