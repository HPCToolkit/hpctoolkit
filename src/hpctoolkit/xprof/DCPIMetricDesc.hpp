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
//    DCPIMetricDesc.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    See, in particular, the comments associated with 'DCPIProfile'.
//
//***************************************************************************

#ifndef DCPIMetricDesc_H 
#define DCPIMetricDesc_H

//************************* System Include Files ****************************

#include "inttypes.h"

//*************************** User Include Files ****************************

#include <include/general.h>

#include <lib/ISA/ISATypes.h>

//*************************** Forward Declarations ***************************

//****************************************************************************
// DCPIMetricDesc
//****************************************************************************

// ----------------------------------------------------------

// Following are bit definitions for DCPI metrics for event types
// supported on Alpha 21264a+ (EV67+) processors.
//
// There are two sets of bits that occupy the same space (an implicit
// union): ProfileMe events form one set; non-ProfileMe events form
// the other set.
//
// ProfileMe bits:
//   Bit layout: <ID><counter_bits, attribute_bits, trap_bits>
//
// NonProfileMe (Regular) bits:
//   Bit layout: <ID><counter>
//
// The bits are set in such a way as to make common queries simple and
// fast.  Because each attribute gets two bits (a true and a false
// bit) we can easily find those metrics with certain attribute values
// using a simple bitwise AND comparison.  For example, given several
// 'DCPIMetricDesc', we can find every ProfileMe metric that retired
// and was not mispredicted with a bitwise AND of 'DCPIMetricDesc' and
// (DCPI_MTYPE_PM | DCPI_PM_CNTR_count | DCPI_PM_ATTR_retired_T |
// DCPI_PM_TRAP_mispredict_F) Note that this scheme only handles
// conjuctive queries (bit A AND bit B AND...); disjuctions require
// much more work.

// ----------------------------------------------------------

// Identity (2 bits; bit 0-1): DCPI has two distinct metric types:
// ProfileMe and 'regular' (traditional, non-ProfileMe metrics). (For
// an actual metric, only one of these bits should be set.)

#define DCPI_MTYPE_PM                 0x0000000000000001
#define DCPI_MTYPE_RM                 0x0000000000000002

// ----------------------------------------------------------
  
// ProfileMe counters (6 bits; bits 2-7): The counter name. (There are
// three counters associated with each unique set of attributes^trap;
// we view each counter as a separate metric.) (For an actual metric,
// only one of these bits should be set.)
#define DCPI_PM_CNTR_MASK             0x00000000000000fc

#define DCPI_PM_CNTR_count            0x0000000000000004
#define DCPI_PM_CNTR_inflight         0x0000000000000008
#define DCPI_PM_CNTR_retires          0x0000000000000010
#define DCPI_PM_CNTR_retdelay         0x0000000000000020
#define DCPI_PM_CNTR_bcmisses         0x0000000000000040
#define DCPI_PM_CNTR_replays          0x0000000000000080

// ProfileMe instruction attributes (22 bits; bits 8-29): Many
// attribute bits may be set in any given sample.  Each attribute
// takes two bits to represent its presence or absence.  (For an
// actual metric, only one bit for each attribute should be set.)
#define DCPI_PM_ATTR_MASK             0x000000003fffff00

#define DCPI_PM_ATTR_retired_T        0x0000000000000100
#define DCPI_PM_ATTR_retired_F        0x0000000000000200
#define DCPI_PM_ATTR_taken_T          0x0000000000000400
#define DCPI_PM_ATTR_taken_F          0x0000000000000800
#define DCPI_PM_ATTR_cbrmispredict_T  0x0000000000001000
#define DCPI_PM_ATTR_cbrmispredict_F  0x0000000000002000
#define DCPI_PM_ATTR_valid_T          0x0000000000004000
#define DCPI_PM_ATTR_valid_F          0x0000000000008000
#define DCPI_PM_ATTR_nyp_T            0x0000000000010000
#define DCPI_PM_ATTR_nyp_F            0x0000000000020000
#define DCPI_PM_ATTR_ldstorder_T      0x0000000000040000
#define DCPI_PM_ATTR_ldstorder_F      0x0000000000080000
#define DCPI_PM_ATTR_map_stall_T      0x0000000000100000
#define DCPI_PM_ATTR_map_stall_F      0x0000000000200000
#define DCPI_PM_ATTR_early_kill_T     0x0000000000400000
#define DCPI_PM_ATTR_early_kill_F     0x0000000000800000
#define DCPI_PM_ATTR_late_kill_T      0x0000000001000000
#define DCPI_PM_ATTR_late_kill_F      0x0000000002000000
#define DCPI_PM_ATTR_capped_T         0x0000000004000000
#define DCPI_PM_ATTR_capped_F         0x0000000008000000
#define DCPI_PM_ATTR_twnzrd_T         0x0000000010000000
#define DCPI_PM_ATTR_twnzrd_F         0x0000000020000000

// ProfileMe instruction traps (32 bits; bits 30-61): Exactly one trap
// is set in any given metric.  Each trap takes two bits to represent
// its presence or absence.  (For an actual metric, only one trap bit
// should be set.)
// Note: trap can be used as a synonym for \!notrap.
#define DCPI_PM_TRAP_MASK             0x3fffffffc0000000

#define DCPI_PM_TRAP_notrap_T         0x0000000040000000 
#define DCPI_PM_TRAP_notrap_F         0x0000000080000000
#define DCPI_PM_TRAP_mispredict_T     0x0000000100000000
#define DCPI_PM_TRAP_mispredict_F     0x0000000200000000
#define DCPI_PM_TRAP_replays_T        0x0000000400000000
#define DCPI_PM_TRAP_replays_F        0x0000000800000000
#define DCPI_PM_TRAP_unaligntrap_T    0x0000001000000000
#define DCPI_PM_TRAP_unaligntrap_F    0x0000002000000000
#define DCPI_PM_TRAP_dtbmiss_T        0x0000004000000000
#define DCPI_PM_TRAP_dtbmiss_F        0x0000008000000000
#define DCPI_PM_TRAP_dtb2miss3_T      0x0000010000000000
#define DCPI_PM_TRAP_dtb2miss3_F      0x0000020000000000
#define DCPI_PM_TRAP_dtb2miss4_T      0x0000040000000000
#define DCPI_PM_TRAP_dtb2miss4_F      0x0000080000000000
#define DCPI_PM_TRAP_itbmiss_T        0x0000100000000000
#define DCPI_PM_TRAP_itbmiss_F        0x0000200000000000
#define DCPI_PM_TRAP_arithtrap_T      0x0000400000000000
#define DCPI_PM_TRAP_arithtrap_F      0x0000800000000000
#define DCPI_PM_TRAP_fpdisabledtrap_T 0x0001000000000000
#define DCPI_PM_TRAP_fpdisabledtrap_F 0x0002000000000000
#define DCPI_PM_TRAP_MT_FPCRtrap_T    0x0004000000000000
#define DCPI_PM_TRAP_MT_FPCRtrap_F    0x0008000000000000
#define DCPI_PM_TRAP_dfaulttrap_T     0x0010000000000000
#define DCPI_PM_TRAP_dfaulttrap_F     0x0020000000000000
#define DCPI_PM_TRAP_iacvtrap_T       0x0040000000000000
#define DCPI_PM_TRAP_iacvtrap_F       0x0080000000000000
#define DCPI_PM_TRAP_OPCDECtrap_T     0x0100000000000000
#define DCPI_PM_TRAP_OPCDECtrap_F     0x0200000000000000
#define DCPI_PM_TRAP_interrupt_T      0x0400000000000000
#define DCPI_PM_TRAP_interrupt_F      0x0800000000000000
#define DCPI_PM_TRAP_mchktrap_T       0x1000000000000000
#define DCPI_PM_TRAP_mchktrap_F       0x2000000000000000

// ----------------------------------------------------------

// Non-ProfileMe (regular metric) event types supported on Alpha
// 21264a+ (EV67+) processors
#define DCPI_RM_cycles            0x0000000000000004
#define DCPI_RM_retires           0x0000000000000008
#define DCPI_RM_replaytrap        0x0000000000000010
#define DCPI_RM_bmiss             0x0000000000000020

// ----------------------------------------------------------

// DCPIMetricDesc: A complete description of a DCPI metric for event
// types supported on Alpha 21264a+ (EV67+) processors.
class DCPIMetricDesc {
public:
  typedef uint64_t bitvec_t;

public:
  // A 'DCPIMetricDesc' can be created using either the bit
  // definitions above or a string of the same format used in
  // 'dcpicat'.
  DCPIMetricDesc(bitvec_t bv = 0) : bits(bv) { }
  DCPIMetricDesc(const char* str);
  virtual ~DCPIMetricDesc() { }
  
  DCPIMetricDesc(const DCPIMetricDesc& x) { *this = x; }
  DCPIMetricDesc& operator=(const DCPIMetricDesc& x) { 
    bits = x.bits; 
    return *this;
  }

  // Some simple queries
  bool IsTypeProfileMe() { return IsSet(DCPI_MTYPE_RM); }
  bool IsTypeRegular() { return IsSet(DCPI_MTYPE_PM); }

  // IsValid: If no bits are set, this must be an invalid descriptor
  bool IsValid() const { return bits != 0; }

  // IsSet: Tests to see if all the specified bits are set
  bool IsSet(const bitvec_t bv) const {
    return (bits & bv) == bv;
  }
  bool IsSet(const DCPIMetricDesc& m) const {
    return (bits & m.bits) == m.bits; 
  }
  // Set: Set all the specified bits
  void Set(const bitvec_t bv) {
    bits = bits | bv;
  }
  void Set(const DCPIMetricDesc& m) {
    bits = bits | m.bits;
  }
  // Unset: Clears all the specified bits
  void Unset(const bitvec_t bv) {
    bits = bits & ~bv;
  }
  void Unset(const DCPIMetricDesc& m) {
    bits = bits & ~(m.bits);
  }
  

  void Dump(std::ostream& o = std::cerr);
  void DDump();

private:

protected:
private:  
  bitvec_t bits;
};

// Convert a string describing a DCPI metric into a 'DCPIMetricDesc'.
// The string should have the format used in 'dcpicat'.  The user is
// responsible for memory deallocation.
// Assumed format: 
//   string ::= ProfileMe_sample_set 
//            | ProfileMe_counter
//            | ProfileMe_sample_set:ProfileMe_counter
//            | Regular_counter
//
//   ProfileMe_sample_set ::= bit_value
//                          | ProfileMe_sample_set ^ bit_value
//                          [| any]
//
//   bit_value ::= <Bit Name>
//               | ! <Bit Name>
//               | <Trap Bit Name>
//               | ! <Trap Bit Name>
//

DCPIMetricDesc
String2DCPIMetricDesc(const char* str);

//****************************************************************************

#endif 

