// -*-Mode: C++;-*- // technically C99

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

/* -*-C-*- */

/****************************************************************************
//
// File: 
//    events.c
//
// Purpose:
//    Supported events for profiling.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
//    Adapted from parts of The Visual Profiler by Curtis L. Janssen
//    (events.c).
//    
*****************************************************************************/

#include <string.h>
#include <stdio.h>

#include <papiStdEventDefs.h>
#include <papi.h>

#include "hpcrun_events.h"

papi_event_t hpcrun_event_table[] = {
{ PAPI_L1_DCM,  "PAPI_L1_DCM",  "Level 1 data cache misses" },
{ PAPI_L1_ICM,  "PAPI_L1_ICM",  "Level 1 instruction cache misses" },
{ PAPI_L2_DCM,  "PAPI_L2_DCM",  "Level 2 data cache misses" },
{ PAPI_L2_ICM,  "PAPI_L2_ICM",  "Level 2 instruction cache misses" },
{ PAPI_L3_DCM,  "PAPI_L3_DCM",  "Level 3 data cache misses" },
{ PAPI_L3_ICM,  "PAPI_L3_ICM",  "Level 3 instruction cache misses" },
{ PAPI_L1_TCM,  "PAPI_L1_TCM",  "Level 1 total cache misses" },
{ PAPI_L2_TCM,  "PAPI_L2_TCM",  "Level 2 total cache misses" },
{ PAPI_L3_TCM,  "PAPI_L3_TCM",  "Level 3 total cache misses" },
{ PAPI_CA_SNP,  "PAPI_CA_SNP",  "Snoops" },
{ PAPI_CA_SHR,  "PAPI_CA_SHR",  "Request access to shared cache line (SMP)" },
{ PAPI_CA_CLN,  "PAPI_CA_CLN",  "Request access to clean cache line (SMP)" },
{ PAPI_CA_INV,  "PAPI_CA_INV",  "Cache Line Invalidation (SMP)" },
{ PAPI_CA_ITV,  "PAPI_CA_ITV",  "Cache Line Intervention (SMP)" },
{ PAPI_L3_LDM,  "PAPI_L3_LDM",  "Level 3 load misses " },
{ PAPI_L3_STM,  "PAPI_L3_STM",  "Level 3 store misses " },
{ PAPI_BRU_IDL, "PAPI_BRU_IDL", "Cycles branch units are idle" },
{ PAPI_FXU_IDL, "PAPI_FXU_IDL", "Cycles integer units are idle" },
{ PAPI_FPU_IDL, "PAPI_FPU_IDL", "Cycles floating point units are idle" },
{ PAPI_LSU_IDL, "PAPI_LSU_IDL", "Cycles load/store units are idle" },
{ PAPI_TLB_DM,  "PAPI_TLB_DM",  "Data translation lookaside buffer misses" },
{ PAPI_TLB_IM,  "PAPI_TLB_IM",  "Instr translation lookaside buffer misses" },
{ PAPI_TLB_TL,  "PAPI_TLB_TL",  "Total translation lookaside buffer misses" },
{ PAPI_L1_LDM,  "PAPI_L1_LDM",  "Level 1 load misses " },
{ PAPI_L1_STM,  "PAPI_L1_STM",  "Level 1 store misses " },
{ PAPI_L2_LDM,  "PAPI_L2_LDM",  "Level 2 load misses " },
{ PAPI_L2_STM,  "PAPI_L2_STM",  "Level 2 store misses " },
{ PAPI_L1_DCH,  "PAPI_L1_DCH",  "Level 1 D cache hits" },
{ PAPI_L2_DCH,  "PAPI_L2_DCH",  "Level 2 D cache hits" },
{ PAPI_L3_DCH,  "PAPI_L3_DCH",  "Level 3 D cache hits" },
{ PAPI_TLB_SD,  "PAPI_TLB_SD",  "TLB shootdowns" },
{ PAPI_CSR_FAL, "PAPI_CSR_FAL", "Failed store conditional instructions" },
{ PAPI_CSR_SUC, "PAPI_CSR_SUC", "Successful store conditional instructions" },
{ PAPI_CSR_TOT, "PAPI_CSR_TOT", "Total store conditional instructions" },
{ PAPI_MEM_SCY, "PAPI_MEM_SCY", "Cycles Stalled Waiting for Memory Access" },
{ PAPI_MEM_RCY, "PAPI_MEM_RCY", "Cycles Stalled Waiting for Memory Read" },
{ PAPI_MEM_WCY, "PAPI_MEM_WCY", "Cycles Stalled Waiting for Memory Write" },
{ PAPI_STL_ICY, "PAPI_STL_ICY", "Cycles with No Instruction Issue" },
{ PAPI_FUL_ICY, "PAPI_FUL_ICY", "Cycles with Maximum Instruction Issue" },
{ PAPI_STL_CCY, "PAPI_STL_CCY", "Cycles with No Instruction Completion" },
{ PAPI_FUL_CCY, "PAPI_FUL_CCY", "Cycles with Maximum Instruction Completion" },
{ PAPI_HW_INT,  "PAPI_HW_INT",  "Hardware interrupts " },
{ PAPI_BR_UCN,  "PAPI_BR_UCN",  "Unconditional branch instructions executed" },
{ PAPI_BR_CN,   "PAPI_BR_CN",   "Conditional branch instructions executed" },
{ PAPI_BR_TKN,  "PAPI_BR_TKN",  "Conditional branch instructions taken" },
{ PAPI_BR_NTK,  "PAPI_BR_NTK",  "Conditional branch instructions not taken" },
{ PAPI_BR_MSP,  "PAPI_BR_MSP",  "Conditional branch instructions mispred" },
{ PAPI_BR_PRC,  "PAPI_BR_PRC",  "Conditional branch instructions corr. pred" },
{ PAPI_FMA_INS, "PAPI_FMA_INS", "FMA instructions completed" },
{ PAPI_TOT_IIS, "PAPI_TOT_IIS", "Total instructions issued" },
{ PAPI_TOT_INS, "PAPI_TOT_INS", "Total instructions executed" },
{ PAPI_INT_INS, "PAPI_INT_INS", "Integer instructions executed" },
{ PAPI_FP_INS,  "PAPI_FP_INS",  "Floating point instructions executed" },
{ PAPI_LD_INS,  "PAPI_LD_INS",  "Load instructions executed" },
{ PAPI_SR_INS,  "PAPI_SR_INS",  "Store instructions executed" },
{ PAPI_BR_INS,  "PAPI_BR_INS",  "Total branch instructions executed" },
{ PAPI_VEC_INS, "PAPI_VEC_INS", "Vector/SIMD instructions executed" },
{ PAPI_FLOPS,   "PAPI_FLOPS",   "Floating Point instructions per second" },
{ PAPI_RES_STL, "PAPI_RES_STL", "Any resource stalls" },
{ PAPI_FP_STAL, "PAPI_FP_STAL", "FP units are stalled " },
{ PAPI_TOT_CYC, "PAPI_TOT_CYC", "Total cycles" },
{ PAPI_IPS,     "PAPI_IPS",     "Instructions executed per second" },
{ PAPI_LST_INS, "PAPI_LST_INS", "Total load/store inst. executed" },
{ PAPI_SYC_INS, "PAPI_SYC_INS", "Synchronization instructions executed" },

#if 0 /* Begin Rice I2 additions (FIXME) */
{ ITA2_BACK_END_BUBBLE_ALL, "ITA2_BACK_END_BUBBLE_ALL", "<fixme>" },
#endif /* End Rice I2 additions */

{ -1,            NULL,          "Invalid event type" }
};

/****************************************************************************/

const papi_event_t *
hpcrun_event_by_name(const char *name)
{ 
  papi_event_t *i = hpcrun_event_table;
  for (; i->name != NULL; i++) {
    if (strcmp(name, i->name) == 0) return i;
  }
  return NULL;
}

const papi_event_t *
hpcrun_event_by_code(int code)
{
  papi_event_t *i = hpcrun_event_table;
  for (; i->name != NULL; i++) {
   if (i->code == code) return i;
  }
  return NULL;
}

/****************************************************************************/

/*
 * output an indented comma-separated list with lines wrapped around 
 * 70 characters 
 */
void 
hpcrun_write_wrapped_event_list(FILE* fs, const papi_event_t* e)
{
  /* initial values */
  static char *sep = "  ";
  static int linelen = 0;

  if (!e) { return; }

  /* Prospective line length (see format string below) */
  linelen += strlen(sep);
  linelen += strlen(e->name);
      
  /* If the prospective line is beyond our line limit, wrap and try
     again.  We are guaranteed that the event name is not longer than
     our line limit (causing an infinite loop).
   */
  if (linelen > 70) {
    fputs("\n", fs);
    sep = "  ";
    linelen = 0;
    hpcrun_write_wrapped_event_list(fs, e);
    return;
  }

  fprintf(fs, "%s%s", sep, e->name);

  sep = ", "; /* default separator to continue next line */
}

/*
  Write the event and its description to stream 'fs'
*/
void 
hpcrun_write_event(FILE* fs, const papi_event_t* e)
{
  if (!e) { return; }
  
  fprintf(fs, "  %s - %s\n", e->name, e->description);
}
