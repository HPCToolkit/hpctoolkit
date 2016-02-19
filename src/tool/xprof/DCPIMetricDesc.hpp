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

#include <string>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/isa/ISATypes.hpp>

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
//   Bit layout: <type>[<counter><attribute><trap>]
//
// NonProfileMe (Regular) bits:
//   Bit layout: <type><counter>
//
// The bits are set in such a way as to make certain common
// ProfileMe-based queries simple and fast.  

// For example, given several 'DCPIMetricDesc', suppose we want to
// find every ProfileMe metric that retired and was not mispredicted.
// We can easily find every such metric using a simple bitwise AND
// comparison with the 'DCPIMetricDesc' and the following query
// expression:
//   (DCPI_MTYPE_PM | DCPI_PM_CNTR_count | DCPI_PM_ATTR_retired_T |
//   DCPI_PM_TRAP_mispredict_F) 
//
// Because each attribute gets two bits (a true and a false bit) we
// can easily find those metrics with certain attribute values.  Note
// that this scheme only allows simple implementation of *conjuctive*
// query expressions; disjuctions will require much more work.

// ----------------------------------------------------------

// Type (2 bits; bit 0-1): DCPI has two distinct metric types:
// ProfileMe and 'regular' (traditional, non-ProfileMe metrics). (For
// an actual metric, only one of these bits should be set.)

#define DCPI_MTYPE_PM                 UINT64_C(0x0000000000000001)
#define DCPI_MTYPE_RM                 UINT64_C(0x0000000000000002)

// ----------------------------------------------------------
  
// ProfileMe counters (6 bits; bits 2-7): The counter name. (There are
// three counters associated with each unique set of attributes^trap;
// we view each counter as a separate metric.) (For an actual metric,
// only one of these bits should be set.)
#define DCPI_PM_CNTR_MASK             UINT64_C(0x00000000000000fc)

#define DCPI_PM_CNTR_count            UINT64_C(0x0000000000000004)
#define DCPI_PM_CNTR_inflight         UINT64_C(0x0000000000000008)
#define DCPI_PM_CNTR_retires          UINT64_C(0x0000000000000010)
#define DCPI_PM_CNTR_retdelay         UINT64_C(0x0000000000000020)
#define DCPI_PM_CNTR_bcmisses         UINT64_C(0x0000000000000040)
#define DCPI_PM_CNTR_replays          UINT64_C(0x0000000000000080)

// ProfileMe instruction attributes (22 bits; bits 8-29): Many
// attribute bits may be set in any given sample.  Each attribute
// takes two bits to represent its presence or absence.  (For an
// actual metric, only one bit for each attribute should be set.)
#define DCPI_PM_ATTR_MASK            UINT64_C(0x000000003fffff00)

#define DCPI_PM_ATTR_retired_T       UINT64_C(0x0000000000000100)
#define DCPI_PM_ATTR_retired_F       UINT64_C(0x0000000000000200)
#define DCPI_PM_ATTR_taken_T         UINT64_C(0x0000000000000400)
#define DCPI_PM_ATTR_taken_F         UINT64_C(0x0000000000000800)
#define DCPI_PM_ATTR_cbrmispredict_T UINT64_C(0x0000000000001000)
#define DCPI_PM_ATTR_cbrmispredict_F UINT64_C(0x0000000000002000)
#define DCPI_PM_ATTR_valid_T         UINT64_C(0x0000000000004000)
#define DCPI_PM_ATTR_valid_F         UINT64_C(0x0000000000008000)
#define DCPI_PM_ATTR_nyp_T           UINT64_C(0x0000000000010000)
#define DCPI_PM_ATTR_nyp_F           UINT64_C(0x0000000000020000)
#define DCPI_PM_ATTR_ldstorder_T     UINT64_C(0x0000000000040000)
#define DCPI_PM_ATTR_ldstorder_F     UINT64_C(0x0000000000080000)
#define DCPI_PM_ATTR_map_stall_T     UINT64_C(0x0000000000100000)
#define DCPI_PM_ATTR_map_stall_F     UINT64_C(0x0000000000200000)
#define DCPI_PM_ATTR_early_kill_T    UINT64_C(0x0000000000400000)
#define DCPI_PM_ATTR_early_kill_F    UINT64_C(0x0000000000800000)
#define DCPI_PM_ATTR_late_kill_T     UINT64_C(0x0000000001000000)
#define DCPI_PM_ATTR_late_kill_F     UINT64_C(0x0000000002000000)
#define DCPI_PM_ATTR_capped_T        UINT64_C(0x0000000004000000)
#define DCPI_PM_ATTR_capped_F        UINT64_C(0x0000000008000000)
#define DCPI_PM_ATTR_twnzrd_T        UINT64_C(0x0000000010000000)
#define DCPI_PM_ATTR_twnzrd_F        UINT64_C(0x0000000020000000)

// ProfileMe instruction traps (16 bits; bits 32-47): Exactly one trap
// (or notrap, meaning none of the other traps) is set in any given
// metric. (For an actual metric, only one trap bit should be set.)
#define DCPI_PM_TRAP_MASK            UINT64_C(0x0000ffff00000000)

#define DCPI_PM_TRAP_notrap          UINT64_C(0x0000000100000000)
#define DCPI_PM_TRAP_mispredict      UINT64_C(0x0000000200000000)
#define DCPI_PM_TRAP_replays         UINT64_C(0x0000000400000000)
#define DCPI_PM_TRAP_unaligntrap     UINT64_C(0x0000000800000000)
#define DCPI_PM_TRAP_dtbmiss         UINT64_C(0x0000001000000000)
#define DCPI_PM_TRAP_dtb2miss3       UINT64_C(0x0000002000000000)
#define DCPI_PM_TRAP_dtb2miss4       UINT64_C(0x0000004000000000)
#define DCPI_PM_TRAP_itbmiss         UINT64_C(0x0000008000000000)
#define DCPI_PM_TRAP_arithtrap       UINT64_C(0x0000010000000000)
#define DCPI_PM_TRAP_fpdisabledtrap  UINT64_C(0x0000020000000000)
#define DCPI_PM_TRAP_MT_FPCRtrap     UINT64_C(0x0000040000000000)
#define DCPI_PM_TRAP_dfaulttrap      UINT64_C(0x0000080000000000)
#define DCPI_PM_TRAP_iacvtrap        UINT64_C(0x0000100000000000)
#define DCPI_PM_TRAP_OPCDECtrap      UINT64_C(0x0000200000000000)
#define DCPI_PM_TRAP_interrupt       UINT64_C(0x0000400000000000)
#define DCPI_PM_TRAP_mchktrap        UINT64_C(0x0000800000000000)

// The negative versions of these require setting every bit except the
// referenced one.
#define DCPI_PM_TRAP_trap          (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_notrap)
#define DCPI_PM_TRAP_N_notrap      DCPI_PM_TRAP_trap

#define DCPI_PM_TRAP_N_mispredict  (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_mispredict)
#define DCPI_PM_TRAP_N_replays     (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_replays)
#define DCPI_PM_TRAP_N_unaligntrap (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_unaligntrap)
#define DCPI_PM_TRAP_N_dtbmiss     (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_dtbmiss)
#define DCPI_PM_TRAP_N_dtb2miss3   (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_dtb2miss3)
#define DCPI_PM_TRAP_N_dtb2miss4   (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_dtb2miss4)
#define DCPI_PM_TRAP_N_itbmiss     (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_itbmiss)
#define DCPI_PM_TRAP_N_arithtrap   (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_arithtrap)
#define DCPI_PM_TRAP_N_fpdisabledtrap (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_fpdisabledtrap)
#define DCPI_PM_TRAP_N_MT_FPCRtrap (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_MT_FPCRtrap)
#define DCPI_PM_TRAP_N_dfaulttrap  (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_dfaulttrap)
#define DCPI_PM_TRAP_N_iacvtrap    (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_iacvtrap)
#define DCPI_PM_TRAP_N_OPCDECtrap  (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_OPCDECtrap)
#define DCPI_PM_TRAP_N_interrupt   (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_interrupt)
#define DCPI_PM_TRAP_N_mchktrap    (DCPI_PM_TRAP_MASK & ~DCPI_PM_TRAP_mchktrap)


// ----------------------------------------------------------

// Non-ProfileMe (regular metric) event types supported on Alpha
// 21264a+ (EV67+) processors (4 bits; bits 2-5)
#define DCPI_RM_CNTR_MASK         UINT64_C(0x000000000000003c)

#define DCPI_RM_cycles            UINT64_C(0x0000000000000004)
#define DCPI_RM_retires           UINT64_C(0x0000000000000008)
#define DCPI_RM_replaytrap        UINT64_C(0x0000000000000010)
#define DCPI_RM_bmiss             UINT64_C(0x0000000000000020)

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
  DCPIMetricDesc(const std::string& str);
  virtual ~DCPIMetricDesc() { }
  
  DCPIMetricDesc(const DCPIMetricDesc& x) { *this = x; }
  DCPIMetricDesc& operator=(const DCPIMetricDesc& x) { 
    bits = x.bits; 
    return *this;
  }

  // Some simple queries
  bool IsTypeProfileMe() { return IsSet(DCPI_MTYPE_PM); }
  bool IsTypeRegular() { return IsSet(DCPI_MTYPE_RM); }

  // IsValid: If no bits are set, this must be an invalid descriptor
  bool IsValid() const { return bits != 0; }

  // IsSet: Tests to see if *all* the specified bits are set
  bool IsSet(const bitvec_t bv) const {
    return (bits & bv) == bv;
  }
  bool IsSet(const DCPIMetricDesc& m) const {
    return (bits & m.bits) == m.bits; 
  }
  // IsSetAny: Tests to see if *any* of the specified bits are set
  bool IsSetAny(const bitvec_t bv) const {
    return (bits & bv) != 0;
  }
  bool IsSetAny(const DCPIMetricDesc& m) const {
    return (bits & m.bits) != 0; 
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
  void Ctor(const char* str);

protected:
  bitvec_t bits;
private:  
};


// Convert a string describing a DCPI metric into a 'DCPIMetricDesc'.
// The string should have the format used in 'dcpicat'.  The following
// is a slightly modified format used by the DCPI tools (cf. ProfileMe
// man page: man dcpiprofileme).
//
// Assumed format: 
//   string ::= ProfileMe_sample_set *
//            | ProfileMe_counter *
//            | ProfileMe_sample_set:ProfileMe_counter
//            | Regular_counter
//
//   ProfileMe_sample_set ::= bit_value
//                          | ProfileMe_sample_set ^ bit_value
//                        { | any }+
//
//   bit_value ::= <Attr Bit Name>
//               | ! <Attr Bit Name>
//               | <Trap Bit Name>
//               | ! <Trap Bit Name>
//
//   Note: trap can be used as a synonym for !notrap.
//
// Notes: 
//   * Note that by themselves these are invalid metrics.  However we
//     allow the parsing of subexpressions that can be spliced together to
//     form a valid event.
//   + We do not support this.
//  

DCPIMetricDesc
String2DCPIMetricDesc(const char* str);

//****************************************************************************

#endif 

