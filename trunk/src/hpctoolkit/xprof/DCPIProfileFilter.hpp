// $Id$
// -*-C++-*-
// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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
//    DCPIProfileFilter.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    See, in particular, the comments associated with 'DCPIProfile'.
//
//***************************************************************************

#ifndef DCPIProfileFilter_H 
#define DCPIProfileFilter_H

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <include/general.h>

#include "PCProfileFilter.h"
#include "DCPIMetricDesc.h"

#include <lib/ISA/ISATypes.h>
#include <lib/binutils/LoadModule.h>

//*************************** Forward Declarations ***************************

class DCPIMetricFilter;

//****************************************************************************

PCProfileFilter* 
GetPredefinedDCPIFilter(const char* metric, LoadModule* lm);

//****************************************************************************
// DCPIMetricExpr
//****************************************************************************

// 'DCPIMetricExpr' extensions to 'DCPIMetricDesc' for precisely
// representing DCPI metrics.
class DCPIMetricExpr : public DCPIMetricDesc {
public:
  DCPIMetricExpr(DCPIMetricDesc::bitvec_t bv)
    : DCPIMetricDesc(bv) { }
  ~DCPIMetricExpr() { }

  DCPIMetricExpr(const DCPIMetricExpr& x) { *this = x; }
  DCPIMetricExpr& operator=(const DCPIMetricExpr& x)
  {
    DCPIMetricDesc::operator=(x);
    return *this;
  }
    
protected:
private:  

};

//****************************************************************************
// DCPIMetricFilter
//****************************************************************************

// DCPIMetricFilter: A DCPIMetricFilter divides metrics into two sets
// sets, 'in' and 'out'.
class DCPIMetricFilter : public MetricFilter {
public:
  DCPIMetricFilter(DCPIMetricExpr expr_) : expr(expr_) { }
  virtual ~DCPIMetricFilter() { }
  
  // Returns true if 'm' satisfies 'expr'; false otherwise.
  virtual bool operator()(const PCProfileMetric* m);

private:
  DCPIMetricExpr expr;
};

//****************************************************************************
// PredefinedDCPIMetricTable
//****************************************************************************

class PredefinedDCPIMetricTable {
public:

  struct Entry {
    const char* name; 
    const char* description;
    
    uint32_t avail; // condition that must be true for metric to be available
    
    DCPIMetricExpr mexpr; // the metric filter expr
    InsnClassExpr iexpr;  // the instruction filter expr
  };

  // Note: these availability tags refer to the raw DCPI metrics (not derived)
  // (We use enum instead of #define b/c we know they only need to fit in
  // 32 bits.)  RM should never be used at the same time as PM0-PM3.
  enum AvailTag {
    RM  = 0x00000001, // the given non ProfileMe (regular) metric must be available
    PM0 = 0x00000002, // ProfileMe mode 0
    PM1 = 0x00000004, // ProfileMe mode 1
    PM2 = 0x00000008, // ProfileMe mode 2
    PM3 = 0x00000010  // ProfileMe mode 3
  };

public:
  PredefinedDCPIMetricTable() { }
  ~PredefinedDCPIMetricTable() { }

  static Entry* FindEntry(const char* token);
  static Entry* Index(uint i);
  static uint GetSize() { return size; }

private:
  // Should not be used 
  PredefinedDCPIMetricTable(const PredefinedDCPIMetricTable& p) { }
  PredefinedDCPIMetricTable& operator=(const PredefinedDCPIMetricTable& p) { return *this; }
  
private:
  static Entry table[];
  static uint size;
  static bool sorted;
};

//****************************************************************************

#endif

