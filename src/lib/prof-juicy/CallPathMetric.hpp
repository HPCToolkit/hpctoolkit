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

//***************************************************************************
//
// File:
//   $Source$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef prof_juicy_CallPathMetric 
#define prof_juicy_CallPathMetric

//************************* System Include Files ****************************//

#include <iostream>

//*************************** User Include Files ****************************//

#include <include/general.h> 

//*************************** Forward Declarations **************************//

//***************************************************************************//
// CSProfMetric
//***************************************************************************//

class CSProfileMetric
{
public:
  CSProfileMetric() : m_period(0) { }
  ~CSProfileMetric() { }

  CSProfileMetric(const CSProfileMetric& x)
    : m_name(x.m_name), m_description(x.m_description), 
      m_flags(x.m_flags), m_period(x.m_period) { }

  CSProfileMetric& operator=(const CSProfileMetric& x) {
    if (this != &x) {
      m_name        = x.m_name;
      m_description = x.m_description;
      m_flags       = x.m_flags;
      m_period      = x.m_period;
    }
    return *this;
  }

  // Name, Description: The metric name and a description
  // Period: The sampling period (whether event or instruction based)
  const std::string& name() const        { return m_name; }
  void               name(const char* x) { m_name = (x) ? x : ""; }

  const std::string& description() const { return m_description; }
  void description(const char* x) { m_description = (x) ? x : ""; }

  unsigned int flags() const         { return m_flags; }
  void         flags(unsigned int x) { m_flags = x; }

  unsigned int period() const         { return m_period; }
  void         period(unsigned int x) { m_period = x; }

  void dump(std::ostream& os = std::cerr) const { }
  void ddump() const { }

protected:
private:  
  std::string  m_name;
  std::string  m_description;
  unsigned int m_flags;  // flags of the metric
  unsigned int m_period; // sampling period
};

//***************************************************************************

#endif /* prof_juicy_CallPathMetric */
