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

#include <include/uint.h>

#include "PCProfileFilter.hpp"
#include "DCPIMetricDesc.hpp"

#include <lib/isa/ISATypes.hpp>

#include <lib/binutils/LM.hpp>

//*************************** Forward Declarations ***************************

class DCPIMetricFilter;

//****************************************************************************

// GetPredefinedDCPIFilter: Given a metric name, returns a DCPI filter
// if available, or NULL.  The user is responsible for unallocating
// the returned object.
PCProfileFilter* 
GetPredefinedDCPIFilter(const char* metric, binutils::LM* lm);

//****************************************************************************
// DCPIMetricExpr
//****************************************************************************

// 'DCPIMetricExpr' extensions to 'DCPIMetricDesc' for representing
// DCPI metric query expressions.  DCPIMetricDesc bits should be used
// to construct an expression.
//
// The following expressions are allowed:
//   * Non-ProfileMe expressions
//   * ProfileMe expressions
// Following is a description of valid query expressions.  Expressions
// are not checked for validity; invalid expressions will report
// non-sensical matches or none at all.  See documentation for
// DCPIMetricDesc bits for more information on bits and their
// meanings.
//
// Non-ProfileMe expressions: <type> && <counter> 
//
// A conjuction between <type> and <counter> subexpressions.  <type>
// must be DCPI_MTYPE_RM.  One and only one counter should be referenced
// within <counter>.  E.g.:
//   valid:   DCPI_MTYPE_RM | DCPI_RM_cycles 
//   invalid: DCPI_MTYPE_RM | DCPI_RM_cycles | DCPI_RM_retires
//
// ProfileMe expressions: <type> && <counter> && <attribute> && <trap>
//
// A conjuction between <type>, <counter>, <attribute> and <trap>
// subexpressions.  <type> must be DCPI_MTYPE_PM.  <counter> should
// reference one ProfileMe counter.  <attribute> is itself a
// conjunction of ProfileMe instruction attributes and may be empty.
// Note that setting both the T and F bit for the same attribute will
// produce no matches.  Because only one ProfileMe trap bit is set for
// any sample, the <trap> subexpression is a *disjunction* of trap
// bits; it may be empty.
//   valid: DCPI_MTYPE_PM | DCPI_PM_CNTR_count | DCPI_PM_ATTR_retired_T
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
  
  // IsSatisfied: Test to see if this query expression is satisfied by
  // the DCPI metric 'm'.
  bool IsSatisfied(const DCPIMetricDesc::bitvec_t bv) {
    return IsSatisfied(DCPIMetricDesc(bv));
  }
  bool IsSatisfied(const DCPIMetricDesc& m) {
    // FIXME: This works for now, but it is technically incorrect
    // becuase regular metrics do not have the <trap> subexpression.

    // The <trap> subexpression is a disjuction, so we test it separately
    bool expr = m.IsSet(bits & ~DCPI_PM_TRAP_MASK);
    bitvec_t trapbv = bits & DCPI_PM_TRAP_MASK;
    if (trapbv != 0) { 
      return expr && m.IsSetAny(trapbv);
    } else {
      return expr;
    }
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
    const char* availStr; // optional extension to above condition
    
    DCPIMetricExpr mexpr; // the metric filter expr
    InsnClassExpr iexpr;  // the instruction filter expr
  };

  // Note: these availability tags refer to the raw DCPI metrics (not
  // derived) (We use enum instead of #define b/c symbolic info is
  // nice and we know they only need to fit in 32 bits.)  RM should
  // never be used at the same time as PM0-PM3.
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
  static Entry* Index(unsigned int i);
  static unsigned int GetSize() { return size; }

private:
  // Should not be used 
  PredefinedDCPIMetricTable(const PredefinedDCPIMetricTable& p) { }
  PredefinedDCPIMetricTable& operator=(const PredefinedDCPIMetricTable& p) { return *this; }
  
private:
  static Entry table[];
  static unsigned int size;
  static bool sorted;
};

//****************************************************************************

#endif

