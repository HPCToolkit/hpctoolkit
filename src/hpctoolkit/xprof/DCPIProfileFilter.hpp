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

//*************************** Forward Declarations ***************************

class DCPIMetricFilter;

//****************************************************************************

// Routines that return predeined metric filters 
namespace DCPIProfileFilter {
  
  PCProfileFilter* PM_Retired();

  // Metrics
  DCPIMetricFilter* PMMetric_Retired();
}

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
// AlphaInsnDesc
//****************************************************************************

// Note: To me moved to ISA

// AlphaInsnDesc: A complete description of a DCPI metric for event
// types supported on Alpha 21264a+ (EV67+) processors.
class AlphaInsnDesc {
public:
  typedef uint32_t bitvec_t;

public:
  // A 'AlphaInsnDesc' can be created using either the bit
  // definitions above or a string of the same format used in
  // 'dcpicat'.
  AlphaInsnDesc(bitvec_t bv = 0) : bits(bv) { }
  virtual ~AlphaInsnDesc() { }
  
  AlphaInsnDesc(const AlphaInsnDesc& x) { *this = x; }
  AlphaInsnDesc& operator=(const AlphaInsnDesc& x) { 
    bits = x.bits; 
    return *this;
  }

  // IsValid: If no bits are set, this must be an invalid descriptor
  bool IsValid() const { return bits != 0; }

  // IsSet: Tests to see if all the specified bits are set
  bool IsSet(const bitvec_t bv) const {
    return (bits & bv) == bv;
  }
  bool IsSet(const AlphaInsnDesc& m) const {
    return (bits & m.bits) == m.bits; 
  }
  // Set: Set all the specified bits
  void Set(const bitvec_t bv) {
    bits = bits | bv;
  }
  void Set(const AlphaInsnDesc& m) {
    bits = bits | m.bits;
  }
  // Unset: Clears all the specified bits
  void Unset(const bitvec_t bv) {
    bits = bits & ~bv;
  }
  void Unset(const AlphaInsnDesc& m) {
    bits = bits & ~(m.bits);
  }

  void Dump(std::ostream& o = std::cerr);
  void DDump();

private:

protected:
private:  
  bitvec_t bits;
};

//****************************************************************************
// AlphaInsnClassExpr
//****************************************************************************

// 'AlphaInsnClassExpr' extensions to 'AlphaInsnDesc' for precisely
// representing Alpha instruction classes.
class AlphaInsnClassExpr : public AlphaInsnDesc
{
public:
  AlphaInsnClassExpr() { }
  ~AlphaInsnClassExpr() { }
  
  AlphaInsnClassExpr(const AlphaInsnClassExpr& x) { *this = x; }
  AlphaInsnClassExpr& operator=(const AlphaInsnClassExpr& x) { 
    AlphaInsnDesc::operator=(x);
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
// DCPIInsnFilter
//****************************************************************************

class LoadModule; // FIXME

// DCPIInsnFilter: Divides PCs into two sets by Alpha instruction class.
class DCPIInsnFilter : public PCFilter {
public:
  DCPIInsnFilter(AlphaInsnClassExpr* expr, LoadModule* lm) { }
  virtual ~DCPIInsnFilter() { }
  
  // Returns true if 'pc' satisfies 'expr'; false otherwise.
  virtual bool operator()(Addr pc);

private:
};


//****************************************************************************

#endif

