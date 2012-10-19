// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/thread_data.c $
// $Id: thread_data.c 3956 2012-10-11 06:41:18Z krentel $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2012, Rice University
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

//
//

//************************* System Include Files ****************************

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

//************************ libmonitor Include Files *************************

#include <monitor.h>

//*************************** User Include Files ****************************

#include "newmem.h"
#include "epoch.h"
#include "handling_sample.h"

#include "cpu_data.h"
#include <lib/prof-lean/spinlock.h>

#include <lush/lush-pthread.h>
#include <messages/messages.h>
#include <trampoline/common/trampoline.h>
#include <memory/mmap.h>

//***************************************************************************
#define MAX_CPU 1024

//***************************************************************************
// local variables
spinlock_t cpu_trace_lock[MAX_CPU];
//***************************************************************************

static cpu_data_t* cpu_data[MAX_CPU] = {NULL};

void
hpcrun_cpu_data_init(int cpu)
{

  cpu_data_t* td = cpu_data[cpu];

  // ----------------------------------------
  // memstore for hpcrun_malloc()
  // ----------------------------------------

  td->id = cpu;

  td->last_time_us = 0;

  // ----------------------------------------
  // epoch: loadmap + cct + cct_ctxt
  // ----------------------------------------
  td->epoch = hpcrun_malloc(sizeof(epoch_t));
  td->epoch->csdata_ctxt = NULL;

  // ----------------------------------------
  // cct2metrics map: associate a metric_set with
  //                  a cct node
  // ----------------------------------------
  hpcrun_cct2metrics_init(&(td->cct2metrics_map));

  // ----------------------------------------
  // tracing
  // ----------------------------------------
  td->trace_min_time_us = 0;
  td->trace_max_time_us = 0;

  // ----------------------------------------
  // IO support
  // ----------------------------------------
  td->hpcrun_file  = NULL;
  td->trace_buffer = NULL;
}


void
hpcrun_allocate_cpu_data(int cpu)
{
  if(cpu_data[cpu] == NULL) {
    cpu_data[cpu] = hpcrun_mmap_anon(sizeof(cpu_data_t));
    hpcrun_cpu_data_init(cpu);
    hpcrun_cpu_epoch_init(cpu);
  }
}


cpu_data_t *
hpcrun_get_cpu_data(int cpu)
{
  return cpu_data[cpu];
}

int
hpcrun_get_max_cpu()
{
  return MAX_CPU;
}

// trace lock routines
void
hpcrun_init_cpu_trace_lock()
{
  int i;
  for(i = 0; i < MAX_CPU; i++)
    cpu_trace_lock[i].thelock = 0L;
}

void
hpcrun_set_cpu_trace_lock(int cpu)
{
  spinlock_lock(&cpu_trace_lock[cpu]);
}

void
hpcrun_unset_cpu_trace_lock(int cpu)
{
  spinlock_unlock(&cpu_trace_lock[cpu]);
}
//***************************************************************************
// 
//***************************************************************************
