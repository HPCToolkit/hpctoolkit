// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef _HPCRUN_GA_H_
#define _HPCRUN_GA_H_

//***************************************************************************
// system includes
//***************************************************************************

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

//***************************************************************************
// local includes
//***************************************************************************


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
