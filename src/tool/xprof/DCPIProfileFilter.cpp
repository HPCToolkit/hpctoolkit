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

#include <cstring>

//*************************** User Include Files ****************************

#include "PCProfileFilter.hpp"
#include "DCPIProfileFilter.hpp"
#include "DCPIProfileMetric.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

using std::endl;
using std::hex;
using std::dec;


//****************************************************************************
// PredefinedDCPIMetricTable
//****************************************************************************

#define TABLE_SZ \
   sizeof(PredefinedDCPIMetricTable::table) / sizeof(PredefinedDCPIMetricTable::Entry)



// --------------------------------------------------------------------------
// NOTE: 
//   the availability of non-ProfileMe metrics must be determined
//   by string matching. Thus, the 'availStr' *must* match the metric names
//   found in dcpicat output (DCPIProfile)
// --------------------------------------------------------------------------

PredefinedDCPIMetricTable::Entry PredefinedDCPIMetricTable::table[] = {

  // non-ProfileMe metric
  { "CYCLES", "Processor cycles",
    RM, "cycles",
    DCPIMetricExpr(DCPI_MTYPE_RM | DCPI_RM_cycles),
    InsnClassExpr(INSN_CLASS_ALL)
  },
  
  // -------------------------------------------------------
  // Each ProfileMe metric is marked with the modes (PM mode) 
  // in which it is available
  // -------------------------------------------------------

  // We generally avoid metrics with early_kill set: When a profiled
  // instruction is killed early in the pipeline (early_kill is set),
  // the PC reported by the hardware may be wrong and all counter
  // values and bits other than valid, early_kill, no_trap, and and
  // map_stall may be wrong.

  // available in ProfileMe mode PM1 only
  {"pmRETDEL", "Delays before retire (excludes all cycles prior to fetch).",
   PM1, NULL,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_retdelay 
		  | DCPI_PM_ATTR_early_kill_F),
   InsnClassExpr(INSN_CLASS_ALL)
  },

  // FIXME: Can we cross check with the retire counter for mode 0, 2
  {"INSTRUCT", "Retired Instructions (includes mispredicted branches)",
   PM0 | PM1 | PM2 | PM3, NULL,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count | DCPI_PM_ATTR_retired_T
		  | DCPI_PM_ATTR_early_kill_F),
   InsnClassExpr(INSN_CLASS_ALL)
  },

  {"FLOPS", "Retired FP Instructions (includes mispredicted branches)",
   PM0 | PM1 | PM2 | PM3, NULL,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count | DCPI_PM_ATTR_retired_T
		  | DCPI_PM_ATTR_early_kill_F),
   InsnClassExpr(INSN_CLASS_FLOP)
  },

  {"MEMOPS", "Retired Memory Access Instructions",
   PM0 | PM1 | PM2 | PM3, NULL,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count | DCPI_PM_ATTR_retired_T
		  | DCPI_PM_ATTR_early_kill_F),
   InsnClassExpr(INSN_CLASS_MEMOP)
  },

  {"INTOPS", "Retired Integer Instructions",
   PM0 | PM1 | PM2 | PM3, NULL,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count | DCPI_PM_ATTR_retired_T
		  | DCPI_PM_ATTR_early_kill_F),
   InsnClassExpr(INSN_CLASS_INTOP)
  },

  {"TRAPS", "Instructions causing traps",
   PM0 | PM1 | PM2 | PM3, NULL,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count 
		  | DCPI_PM_ATTR_early_kill_F | DCPI_PM_TRAP_trap),
   InsnClassExpr(INSN_CLASS_ALL)
  },

  {"LSREPLAY", "Replays caused by load/store ordering. [Untested]",
   PM0 | PM1 | PM2 | PM3, NULL,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count 
		  | DCPI_PM_ATTR_ldstorder_T | DCPI_PM_ATTR_early_kill_F),
   InsnClassExpr(INSN_CLASS_ALL)
  },

  {"MPBRANCH", "Mispredicted branches",
   PM0 | PM1 | PM2 | PM3, NULL,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count 
		  | DCPI_PM_ATTR_cbrmispredict_T | DCPI_PM_ATTR_early_kill_F),
   InsnClassExpr(INSN_CLASS_ALL) /* bit is only true for branches */
  },


  {"pmBCMISS", "sampled B-cache (L2) misses.",
   PM0 | PM1 | PM2 | PM3, NULL,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_bcmisses 
		  | DCPI_PM_ATTR_early_kill_F),
   InsnClassExpr(INSN_CLASS_ALL)
  },

  {"TLBMISS", "TLB miss at any level.",
   PM0 | PM1 | PM2 | PM3, NULL,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count 
		  | DCPI_PM_TRAP_dtbmiss | DCPI_PM_TRAP_dtb2miss3 
		  | DCPI_PM_TRAP_dtb2miss4 | DCPI_PM_ATTR_early_kill_F),
   InsnClassExpr(INSN_CLASS_ALL)
  },

  {"IMISS", "Lower bound on instruction cache misses",
   PM0 | PM1 | PM2 | PM3, NULL,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_count 
		  | DCPI_PM_ATTR_nyp_T | DCPI_PM_ATTR_early_kill_F),
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
  {"pmINFLT", "Inflight cycles (excludes fetch stage) for instructions that retired without trapping.",
   PM0 | PM1 | PM3, NULL,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_inflight 
		  | DCPI_PM_ATTR_early_kill_F),
   InsnClassExpr(INSN_CLASS_ALL)
  },


  {"pmRETIRE", "Instruction retires.",
   PM0 | PM2, NULL,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_retires 
		  | DCPI_PM_ATTR_early_kill_F),
   InsnClassExpr(INSN_CLASS_ALL)
  },
  {"pmREPLAY", "Memory system replay traps.",
   PM3, NULL,
   DCPIMetricExpr(DCPI_MTYPE_PM | DCPI_PM_CNTR_replays
		  | DCPI_PM_ATTR_early_kill_F),
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

  { "RETIRES", "Retired instructions",
    RM, "retires",
    DCPIMetricExpr(DCPI_MTYPE_RM | DCPI_RM_retires),
    InsnClassExpr(INSN_CLASS_ALL)
  },

  { "MBREPLAY", "Mbox replay traps",
    RM, "replaytrap",
    DCPIMetricExpr(DCPI_MTYPE_RM | DCPI_RM_replaytrap),
    InsnClassExpr(INSN_CLASS_ALL)
  },

  { "BCMISS", "Bcache misses or long-latency probes",
    RM, "bmiss",
    DCPIMetricExpr(DCPI_MTYPE_RM | DCPI_RM_bmiss),
    InsnClassExpr(INSN_CLASS_ALL)
  }

};

unsigned int PredefinedDCPIMetricTable::size = TABLE_SZ;

bool PredefinedDCPIMetricTable::sorted = false;

#undef TABLE_SZ

//****************************************************************************

PredefinedDCPIMetricTable::Entry*
PredefinedDCPIMetricTable::FindEntry(const char* metric)
{
  // FIXME: we should search a quick-sorted table with binary search.
  // check 'sorted'
  Entry* found = NULL;
  for (unsigned int i = 0; i < GetSize(); ++i) {
    if (strcmp(metric, table[i].name) == 0) {
      found = &table[i];
    }
  }
  return found;
}

PredefinedDCPIMetricTable::Entry*
PredefinedDCPIMetricTable::Index(unsigned int i)
{
  if (i >= GetSize()) { return NULL; }
  return &table[i];
}


//****************************************************************************
// 
//****************************************************************************

PCProfileFilter* 
GetPredefinedDCPIFilter(const char* metric, binutils::LM* lm)
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
  DIAG_Assert(dm, "Internal Error: invalid cast!");
  
  const DCPIMetricDesc& mdesc = dm->GetDCPIDesc();
  return expr.IsSatisfied(mdesc);
}
