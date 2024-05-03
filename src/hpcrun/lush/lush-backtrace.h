// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   LUSH: Logical Unwind Support for HPCToolkit
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, Rice University.
//
//***************************************************************************

#ifndef lush_lush_backtrace_h
#define lush_lush_backtrace_h

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>
#include <ucontext.h>

//*************************** User Include Files ****************************

#include "lush.h"

#include "../epoch.h"

//*************************** Forward Declarations **************************

#ifdef __cplusplus
extern "C" {
#endif

//***************************************************************************
// LUSH Agents
//***************************************************************************

// LUSH support
extern lush_agent_pool_t* lush_agents;
extern bool is_lush_agent;


// TODO: distinguish between logical unwind agents and metric agents

static inline bool
hpcrun_isLogicalUnwind()
{
  return (is_lush_agent);
}


static inline void
hpcrun_logicalUnwind(bool x)
{
  is_lush_agent = x;
}



//***************************************************************************
// LUSH backtrace
//***************************************************************************

cct_node_t*
lush_backtrace2cct(cct_bundle_t* cct, ucontext_t* context,
                   int metricId,
                   hpcrun_metricVal_t metricIncr,
                   int skipInner, int isSync);


// **************************************************************************

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* lush_lush_backtrace_h */
