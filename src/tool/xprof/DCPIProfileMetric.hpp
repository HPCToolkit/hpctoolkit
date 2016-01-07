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
//    DCPIProfileMetric.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    See, in particular, the comments associated with 'DCPIProfile'.
//
//***************************************************************************

#ifndef DCPIProfileMetric_H 
#define DCPIProfileMetric_H

//************************* System Include Files ****************************

#include <string>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "PCProfile.hpp"
#include "DCPIProfileFilter.hpp"

#include <lib/isa/ISATypes.hpp>

//*************************** Forward Declarations ***************************

//****************************************************************************
// DCPIProfileMetric
//****************************************************************************

// 'DCPIProfileMetric' extensions to 'PCProfileMetric' for precisely
// representing DCPI metrics.
class DCPIProfileMetric : public PCProfileMetric {
public:
  // A metric can be created from a 'DCPIMetricDesc' or a string that
  // will be used to create a 'DCPIMetricDesc'.
  DCPIProfileMetric(ISA* isa_) : PCProfileMetric(isa_) { }

  DCPIProfileMetric(ISA* isa_, const char* name) 
    : PCProfileMetric(isa_), desc(name) { }

  DCPIProfileMetric(ISA* isa_, const std::string& name) 
    : PCProfileMetric(isa_), desc(name) { }

  DCPIProfileMetric(ISA* isa_, const DCPIMetricDesc& x) 
    : PCProfileMetric(isa_), desc(x) { }

  ~DCPIProfileMetric() { }
  
  // GetDCPIDesc: 
  const DCPIMetricDesc& GetDCPIDesc() const { return desc; }
  void SetDCPIDesc(const DCPIMetricDesc& x) { 
    desc = x; 
  }
  
  void dump(std::ostream& o = std::cerr);
  void ddump();
  
private:
  // Should not be used  
  DCPIProfileMetric(const DCPIProfileMetric& m);
  DCPIProfileMetric& operator=(const DCPIProfileMetric& m) { return *this; }
  
protected:
private:  
  DCPIMetricDesc desc;
};

//****************************************************************************

#endif 

