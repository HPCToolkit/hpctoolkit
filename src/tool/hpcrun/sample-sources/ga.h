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

#ifndef _HPCRUN_GA_H_
#define _HPCRUN_GA_H_

//***************************************************************************
// system includes
//***************************************************************************

#include <stddef.h>
#include <stdio.h>
#include <string.h>

//***************************************************************************
// local includes
//***************************************************************************

#include <include/uint.h>

//***************************************************************************

#define GA_DataCentric_Prototype 1

//***************************************************************************

extern uint64_t hpcrun_ga_period;

extern int hpcrun_ga_metricId_onesidedOp;
extern int hpcrun_ga_metricId_collectiveOp;
extern int hpcrun_ga_metricId_latency;
extern int hpcrun_ga_metricId_latencyExcess;
extern int hpcrun_ga_metricId_bytesXfr;

//***************************************************************************


#if (GA_DataCentric_Prototype)

#define hpcrun_ga_metricId_dataTblSz 10
#define hpcrun_ga_metricId_dataStrLen 32

extern int hpcrun_ga_metricId_dataTblIdx_next;
extern int hpcrun_ga_metricId_dataTblIdx_max; // exclusive upper bound

typedef struct hpcrun_ga_metricId_dataDesc {
  int metricId;
  char name[hpcrun_ga_metricId_dataStrLen];
} hpcrun_ga_metricId_dataDesc_t;


extern hpcrun_ga_metricId_dataDesc_t hpcrun_ga_metricId_dataTbl[];

static inline hpcrun_ga_metricId_dataDesc_t*
hpcrun_ga_metricId_dataTbl_find(int idx)
{
  return &hpcrun_ga_metricId_dataTbl[idx];
}


int
hpcrun_ga_dataIdx_new(const char* name);

static inline int
hpcrun_ga_dataIdx_isValid(int idx)
{
  return ((idx >= 0) && (idx < hpcrun_ga_metricId_dataTblIdx_next));
}

#endif // GA_DataCentric_Prototype


//***************************************************************************

#endif // _HPCRUN_GA_H_
