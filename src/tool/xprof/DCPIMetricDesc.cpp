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
//    DCPIMetricDesc.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
using std::endl;
using std::hex;
using std::dec;

#include <string>
using std::string;

#include <cstring>

//*************************** User Include Files ****************************

#include "DCPIMetricDesc.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

int
SetDCPIMetricDescBit(const char* token, DCPIMetricDesc& m);


//****************************************************************************
// DCPITranslationTable
//****************************************************************************

class DCPITranslationTable {
public:
  struct Entry {
    const char* tok;
    DCPIMetricDesc::bitvec_t bits;
  };

public:
  DCPITranslationTable() { }
  ~DCPITranslationTable() { }

  static Entry* FindEntry(const char* token);
  static unsigned int GetSize() { return size; }

private:
  // Should not be used 
  DCPITranslationTable(const DCPITranslationTable& p) { }
  DCPITranslationTable& operator=(const DCPITranslationTable& p) { return *this; }
  
private:
  static Entry table[];
  static unsigned int size;
  static bool sorted;
};

//****************************************************************************

#define TYPEPM(m) DCPI_MTYPE_PM | (m)
#define TYPERM(m) DCPI_MTYPE_RM | (m)

// 'pfx', 'n1', 'n2', 'n3' should all be strings
#define DCPI_PM_CNTR(pfx, n1, n2, n3, bit1, bit2, bit3) \
  { pfx n1, TYPEPM((bit1)) }, \
  { pfx n2, TYPEPM((bit2)) }, \
  { pfx n3, TYPEPM((bit3)) }
  /* the macro concatenation operator ## must produce a token */

// 'name' should be a string
#define DCPI_PM_ATTR(name, truebits, falsebits) \
  { name,      TYPEPM((truebits)) }, \
  { "!" name, TYPEPM((falsebits)) }
  /* the macro concatenation operator ## must produce a token */

// 'name' should be a string
#define DCPI_PM_TRAP(name, truebits, falsebits) \
  { name,      TYPEPM((truebits)) }, \
  { "!" name, TYPEPM((falsebits)) }
  /* the macro concatenation operator ## must produce a token */


#define TABLE_SZ \
   sizeof(DCPITranslationTable::table) / sizeof(DCPITranslationTable::Entry)


DCPITranslationTable::Entry DCPITranslationTable::table[] = {
  // ProfileMe counters
  DCPI_PM_CNTR("m0", "count", "inflight", "retires", DCPI_PM_CNTR_count, DCPI_PM_CNTR_inflight, DCPI_PM_CNTR_retires), 
  DCPI_PM_CNTR("m1", "count", "inflight", "retdelay", DCPI_PM_CNTR_count, DCPI_PM_CNTR_inflight, DCPI_PM_CNTR_retdelay), 
  DCPI_PM_CNTR("m2", "count", "retires", "bcmisses", DCPI_PM_CNTR_count, DCPI_PM_CNTR_retires, DCPI_PM_CNTR_bcmisses), 
  DCPI_PM_CNTR("m3", "count", "inflight", "replays", DCPI_PM_CNTR_count, DCPI_PM_CNTR_inflight, DCPI_PM_CNTR_replays), 

  // ProfileMe instruction attributes
  DCPI_PM_ATTR("retired", DCPI_PM_ATTR_retired_T, DCPI_PM_ATTR_retired_F),
  DCPI_PM_ATTR("taken", DCPI_PM_ATTR_taken_T, DCPI_PM_ATTR_taken_F),
  DCPI_PM_ATTR("cbrmispredict", DCPI_PM_ATTR_cbrmispredict_T, DCPI_PM_ATTR_cbrmispredict_F),
  DCPI_PM_ATTR("valid", DCPI_PM_ATTR_valid_T, DCPI_PM_ATTR_valid_F),
  DCPI_PM_ATTR("nyp", DCPI_PM_ATTR_nyp_T, DCPI_PM_ATTR_nyp_F),
  DCPI_PM_ATTR("ldstorder", DCPI_PM_ATTR_ldstorder_T, DCPI_PM_ATTR_ldstorder_F),
  DCPI_PM_ATTR("map_stall", DCPI_PM_ATTR_map_stall_T, DCPI_PM_ATTR_map_stall_F),
  DCPI_PM_ATTR("early_kill", DCPI_PM_ATTR_early_kill_T, DCPI_PM_ATTR_early_kill_F),
  DCPI_PM_ATTR("late_kill", DCPI_PM_ATTR_late_kill_T, DCPI_PM_ATTR_late_kill_F),
  DCPI_PM_ATTR("capped", DCPI_PM_ATTR_capped_T, DCPI_PM_ATTR_capped_F),
  DCPI_PM_ATTR("twnzrd", DCPI_PM_ATTR_twnzrd_T, DCPI_PM_ATTR_twnzrd_F),
  
  // ProfileMe instruction traps  
  { "trap", TYPEPM(DCPI_PM_TRAP_trap) },
  DCPI_PM_TRAP("notrap", DCPI_PM_TRAP_notrap, DCPI_PM_TRAP_N_notrap),
  DCPI_PM_TRAP("mispredict", DCPI_PM_TRAP_mispredict, DCPI_PM_TRAP_N_mispredict),
  DCPI_PM_TRAP("replays", DCPI_PM_TRAP_replays, DCPI_PM_TRAP_N_replays),
  DCPI_PM_TRAP("unaligntrap", DCPI_PM_TRAP_unaligntrap, DCPI_PM_TRAP_N_unaligntrap),
  DCPI_PM_TRAP("dtbmiss", DCPI_PM_TRAP_dtbmiss, DCPI_PM_TRAP_N_dtbmiss),
  DCPI_PM_TRAP("dtb2miss3", DCPI_PM_TRAP_dtb2miss3, DCPI_PM_TRAP_N_dtb2miss3),
  DCPI_PM_TRAP("dtb2miss4", DCPI_PM_TRAP_dtb2miss4, DCPI_PM_TRAP_N_dtb2miss4),
  DCPI_PM_TRAP("itbmiss", DCPI_PM_TRAP_itbmiss, DCPI_PM_TRAP_N_itbmiss),
  DCPI_PM_TRAP("arithtrap", DCPI_PM_TRAP_arithtrap, DCPI_PM_TRAP_N_arithtrap),
  DCPI_PM_TRAP("fpdisabledtrap", DCPI_PM_TRAP_fpdisabledtrap, DCPI_PM_TRAP_N_fpdisabledtrap),
  DCPI_PM_TRAP("MT_FPCRtrap", DCPI_PM_TRAP_MT_FPCRtrap, DCPI_PM_TRAP_N_MT_FPCRtrap),
  DCPI_PM_TRAP("dfaulttrap", DCPI_PM_TRAP_dfaulttrap, DCPI_PM_TRAP_N_dfaulttrap),
  DCPI_PM_TRAP("iacvtrap", DCPI_PM_TRAP_iacvtrap, DCPI_PM_TRAP_N_iacvtrap),
  DCPI_PM_TRAP("OPCDECtrap", DCPI_PM_TRAP_OPCDECtrap, DCPI_PM_TRAP_N_OPCDECtrap),
  DCPI_PM_TRAP("interrupt", DCPI_PM_TRAP_interrupt, DCPI_PM_TRAP_N_interrupt),
  DCPI_PM_TRAP("mchktrap", DCPI_PM_TRAP_mchktrap, DCPI_PM_TRAP_N_mchktrap),
  
  // Non-ProfileMe event types
  { "cycles", TYPERM(DCPI_RM_cycles) },
  { "retires", TYPERM(DCPI_RM_retires) },
  { "replaytrap", TYPERM(DCPI_RM_replaytrap) },
  { "bmiss", TYPERM(DCPI_RM_bmiss) }

};

unsigned int DCPITranslationTable::size = TABLE_SZ;

bool DCPITranslationTable::sorted = false;

#undef TYPEPM
#undef TYPERM
#undef DCPI_PM_CNTR
#undef DCPI_PM_ATTR
#undef DCPI_PM_TRAP
#undef TABLE_SZ

//****************************************************************************

DCPITranslationTable::Entry*
DCPITranslationTable::FindEntry(const char* token)
{
  // FIXME: we should search a quick-sorted table with binary search.
  // check 'sorted'
  Entry* found = NULL;
  for (unsigned int i = 0; i < GetSize(); ++i) {
    if (strcmp(token, table[i].tok) == 0) {
      found = &table[i];
    }
  }
  return found;
}


//****************************************************************************
// DCPIMetricDesc
//****************************************************************************

DCPIMetricDesc::DCPIMetricDesc(const char* str)
{
  Ctor(str);
}


DCPIMetricDesc::DCPIMetricDesc(const std::string& str)
{
  Ctor(str.c_str());
}


void DCPIMetricDesc::Ctor(const char* str)
{
  DCPIMetricDesc m = String2DCPIMetricDesc(str);
  bits = m.bits;
}


// String2DCPIMetricDesc: See header for accepted syntax.  If an error
// occurs, the DCPIMetricDesc::IsValid() will return false.
DCPIMetricDesc
String2DCPIMetricDesc(const char* str)
{
  DCPIMetricDesc m; // initialized to invalid state

  // 0. Test for NULL or empty string
  if (!str || strlen(str) == 0) {
    return m;
  }

  // 1. Special recursive case (should only happen once)
  //    str ::= ProfileMe_sample_set:ProfileMe_counter
  // Note: cannot use strtok because of recursion.
  char* sep = const_cast<char*>(strchr(str, ':')); // separator
  if (sep != NULL) {
    *sep = '\0'; // temporarily modify to get first part
    string sampleset = str;
    *sep = ':';
    string counter = sep+1;
    DCPIMetricDesc m1 = String2DCPIMetricDesc(sampleset.c_str());
    DCPIMetricDesc m2 = String2DCPIMetricDesc(counter.c_str());
	
    if (m1.IsValid() && m2.IsValid()) {
      m.Set(m1);
      m.Set(m2);
    } 
    return m;
  }
  
  // 2. Typical case: 
  //    str ::= ProfileMe_sample_set | ProfileMe_counter | Regular_counter
  //
  // ProfileMe_sample_set may contain multiple tokens (divided by '^'); 
  // otherwise we can only distinguish which type we have by
  // string matching.  Fortunately, this can be a very simple
  // implementation because attribute and counter names are unique.

  DCPIMetricDesc m1;
  char* tok = strtok(const_cast<char*>(str), "^");
  while (tok != NULL) {
    int ret = SetDCPIMetricDescBit(tok, m1);
    if (ret != 0) { return m; }
    
    tok = strtok((char*)NULL, "^");
  }
  m.Set(m1);
  
  return m;
}


// SetDCPIMetricDescBit: Given a DCPI bit value (possibly preceeded by
// ! for negation) or counter name token string, set the appropriate
// bit in 'm'.  Returns non-zero on error.
int
SetDCPIMetricDescBit(const char* token, DCPIMetricDesc& m)
{
  // 0. Test for NULL or empty string
  if (!token || strlen(token) == 0) {
    return 1;
  }
  
  // 1. Set the appropriate bit by string comparisons
  DCPITranslationTable::Entry* e = DCPITranslationTable::FindEntry(token);
  if (!e) { return 1; }
  m.Set(e->bits);
  
  return 0;
}

//****************************************************************************


