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
//    DCPIFilterExpr.C
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

//*************************** User Include Files ****************************

#include "PCProfileFilter.h"
#include "DCPIProfileFilter.h"
#include "DCPIProfileMetric.h"

//*************************** Forward Declarations ***************************

using std::endl;
using std::hex;
using std::dec;


//****************************************************************************
// PredefinedDCPIMetricTable
//****************************************************************************

#define TABLE_SZ \
   sizeof(PredefinedDCPIMetricTable::table) / sizeof(PredefinedDCPIMetricTable::Entry)


PredefinedDCPIMetricTable::Entry PredefinedDCPIMetricTable::table[] = {

  // -------------------------------------------------------
  // Metrics available whenever ProfileMe is used (any PM mode)
  // -------------------------------------------------------

  // FIXME: what should we do about unreliable data and early_kill?

  // NOTE: it seems we can cross check with the retire counter 
  //  for mode 0 or mode 2...
  {"retired_insn", "Retired Instructions (includes mispredicted branches)",
   PM0 | PM1 | PM2 | PM3,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count | DCPI_PM_ATTR_retired_T),
   InsnClassExpr(INSN_CLASS_ALL)
  },

  {"retired_fp_insn", "Retired FP Instructions (includes mispredicted branches)",
   PM0 | PM1 | PM2 | PM3,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count | DCPI_PM_ATTR_retired_T),
   InsnClassExpr(INSN_CLASS_FLOP)
  },

  {"mispredicted_branches", "Mispredicted branches",
   PM0 | PM1 | PM2 | PM3,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count | DCPI_PM_ATTR_cbrmispredict_T),
   InsnClassExpr(INSN_CLASS_ALL) /* bit is only true for branches */
  },

  {"replay_ldst", "Replays caused by load/store ordering. [Untested]",
   PM0 | PM1 | PM2 | PM3,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count | DCPI_PM_ATTR_ldstorder_T),
   InsnClassExpr(INSN_CLASS_ALL)
  },

  {"trapped_insn", "Instructions causing traps",
   PM0 | PM1 | PM2 | PM3,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count | DCPI_PM_TRAP_trap),
   InsnClassExpr(INSN_CLASS_ALL)
  },

#if 0
//   "icache_miss_lb", "Lower bound on icache misses"
//      count + nyp , any insn
#endif

  // -------------------------------------------------------
  // Metrics available for a specific ProfileMe mode
  // -------------------------------------------------------
  // m0: inflight, retires
  // m1: inflight, retdelay
  // m2: retires,  bcmisses
  // m3: inflight, replays

  // FIXME: these are just the raw counters; how best to combine them?
  {"pm_inflight", "Inflight cycles (excludes fetch stage) for instructions that retired without trapping.",
   PM0 | PM1 | PM3,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_inflight),
   InsnClassExpr(INSN_CLASS_ALL)
  },

  {"pm_retdelay", "Delays before retire (excludes all cycles prior to fetch).",
   PM1,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_retdelay),
   InsnClassExpr(INSN_CLASS_ALL)
  },

  {"pm_retires", "Instruction retires.",
   PM0 | PM2,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_retires),
   InsnClassExpr(INSN_CLASS_ALL)
  },

  {"pm_bcmisses", "B-cache (L2) cache misses.",
   PM2,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_bcmisses),
   InsnClassExpr(INSN_CLASS_ALL)
  },

  {"pm_replays", "Memory system replay traps.",
   PM3,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_replays),
   InsnClassExpr(INSN_CLASS_ALL)
  },

#if 0
//  "m0inflight"   "Number of cycles instruction was Inflight
//  "m0retires"    Instruction retires --> cross check retired instructions?

//  "m1inflight"   Inflight cycles
//  "m1retdelay"   Delays Retire delay (excludes all cycles prior to fetch)--> 

//  "m2retires"    Instruction retires --> cross check retired instructions?
//  "m2bcmisses"   B-cache misses --> b-cache misses

//  "m3inflight"   Inflight cycles
//  "m3replays"    Pipeline replay traps
#endif

  // -------------------------------------------------------
  // Non ProfileMe metrics, possibly available at any time
  // -------------------------------------------------------

  { "cntr_cycles", "Processor cycles",
    RM,
    DCPIMetricExpr(DCPI_MTYPE_RM | DCPI_RM_cycles),
    InsnClassExpr(INSN_CLASS_ALL)
  },
  
  { "cntr_retires", "Retired instructions",
    RM,
    DCPIMetricExpr(DCPI_MTYPE_RM | DCPI_RM_retires),
    InsnClassExpr(INSN_CLASS_ALL)
  },

  { "cntr_replaytrap", "Mbox replay traps",
    RM,
    DCPIMetricExpr(DCPI_MTYPE_RM | DCPI_RM_replaytrap),
    InsnClassExpr(INSN_CLASS_ALL)
  },

  { "cntr_bmiss", "Bcache misses or long-latency probes",
    RM,
    DCPIMetricExpr(DCPI_MTYPE_RM | DCPI_RM_bmiss),
    InsnClassExpr(INSN_CLASS_ALL)
  }

};

uint PredefinedDCPIMetricTable::size = TABLE_SZ;

bool PredefinedDCPIMetricTable::sorted = false;

#undef TABLE_SZ

//****************************************************************************

PredefinedDCPIMetricTable::Entry*
PredefinedDCPIMetricTable::FindEntry(const char* metric)
{
  // FIXME: we should search a quick-sorted table with binary search.
  // check 'sorted'
  Entry* found = NULL;
  for (uint i = 0; i < GetSize(); ++i) {
    if (strcmp(metric, table[i].name) == 0) {
      found = &table[i];
    }
  }
  return found;
}

PredefinedDCPIMetricTable::Entry*
PredefinedDCPIMetricTable::Index(uint i)
{
  if (i >= GetSize()) { return NULL; }
  return &table[i];
}


//****************************************************************************
// 
//****************************************************************************

PCProfileFilter* 
GetPredefinedDCPIFilter(const char* metric, LoadModule* lm)
{
  PredefinedDCPIMetricTable::Entry* e =
    PredefinedDCPIMetricTable::FindEntry(metric);
  if (!e) { return NULL; }

  PCProfileFilter* f = new PCProfileFilter(new DCPIMetricFilter(e->mexpr), 
					   new InsnFilter(e->iexpr, lm));
  
  f->SetName(e->name);
  f->SetDescription(e->description);
  return f;
}

//****************************************************************************
// DCPIMetricFilter
//****************************************************************************

bool 
DCPIMetricFilter::operator()(const PCProfileMetric* m)
{
  const DCPIProfileMetric* dm = dynamic_cast<const DCPIProfileMetric*>(m);
  BriefAssertion(dm && "Internal Error: invalid cast!");
  
  const DCPIMetricDesc& mdesc = dm->GetDCPIDesc();
  return expr.IsSatisfied(mdesc);
}
