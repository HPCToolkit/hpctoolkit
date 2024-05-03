// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
// $HeadURL$
//
// Purpose:
// This file generalizes how hpcrun computes the 'rank' of a process,
// eg: MPI, dmapp, gasnet, etc.
//
// For now, this API acts passively, that is, it waits for something
// else in hpcrun to ask what is the rank.  But we may need to
// generalize this to actively find the rank in some cases.
//
// Note: we kinda assume that all methods (if available) return
// consistent answers.
//
//***************************************************************************

#define _GNU_SOURCE

#include <stdint.h>
#include "libmonitor/monitor.h"
#include "../common/lean/OSUtil.h"
#include "rank.h"

struct jobinfo {
  int  version;
  int  hw_version;
  int  npes;
  int  pe;
  long pad[20];
};

int dmapp_get_jobinfo(struct jobinfo *info);

extern int32_t gasneti_mynode;


//***************************************************************************
// interface functions
//***************************************************************************

// Returns: generalized rank, or else -1 if unknown or unavailable.
//
int
hpcrun_get_rank(void)
{
  int rank;

  rank = OSUtil_rank();
  if (rank >= 0) {
    return rank;
  }

  rank = monitor_mpi_comm_rank();
  if (rank >= 0) {
    return rank;
  }

  return -1;
}
