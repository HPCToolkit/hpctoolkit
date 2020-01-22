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
// Copyright ((c)) 2002-2020, Rice University
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

//*****************************************************************************
// system includes
//*****************************************************************************

#include <dlfcn.h>



//*****************************************************************************
// libmonitor
//*****************************************************************************

#include <monitor.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <lib/prof-lean/spinlock.h>

#include <hpcrun/trace.h>
#include <hpcrun/write_data.h>

#include "ompt-defer.h"
#include "ompt-defer-write.h"



//*****************************************************************************
// global variables
//*****************************************************************************

static struct entry_t *unresolved_list = NULL;

static spinlock_t unresolved_list_lock = SPINLOCK_UNLOCKED;



//*****************************************************************************
// interface operations
//*****************************************************************************

struct entry_t *
new_dw_entry
(
 void
)
{
  struct entry_t *entry = NULL;

  entry = (struct entry_t*)hpcrun_malloc(sizeof(struct entry_t));
  entry->prev = entry->next = NULL;
  entry->td = NULL;
  entry->flag = false;
  return entry;
}


void
insert_dw_entry
(
 struct entry_t* entry
)
{
  spinlock_lock(&unresolved_list_lock);
  if(!unresolved_list) {
    unresolved_list = entry;
    spinlock_unlock(&unresolved_list_lock);
    return;
  }
  entry->prev = NULL;
  entry->next = unresolved_list;
  unresolved_list->prev = entry;
  unresolved_list = entry;
  spinlock_unlock(&unresolved_list_lock);
}


void 
add_defer_td
(
 thread_data_t *td
)
{
  struct entry_t *entry = new_dw_entry();
  entry->td = td;
  insert_dw_entry(entry);
}


void
write_other_td
(
 void
)
{
  spinlock_lock(&unresolved_list_lock);
  struct entry_t *entry = unresolved_list;
  spinlock_unlock(&unresolved_list_lock);
  while(entry) {
    if(entry->flag) {
      entry = entry->next;
      continue;
    }
    entry->flag = true;
    thread_data_t *td = hpcrun_get_thread_data();
    cct2metrics_t* store_cct2metrics_map = td->core_profile_trace_data.cct2metrics_map;
    td->core_profile_trace_data.cct2metrics_map = entry->td->core_profile_trace_data.cct2metrics_map;
    if(entry->td->defer_flag) {
      TMSG(DEFER_CTXT, "write another td with id %d", entry->td->core_profile_trace_data.id);
      resolve_cntxt_fini(entry->td);
    }

    // write out a given td
    hpcrun_write_profile_data(&(entry->td->core_profile_trace_data));
    hpcrun_trace_close(&(entry->td->core_profile_trace_data));
    td->core_profile_trace_data.cct2metrics_map = store_cct2metrics_map;

    entry = entry->next;
  }
}
