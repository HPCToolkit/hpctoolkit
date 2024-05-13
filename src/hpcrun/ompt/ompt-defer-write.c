// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// system includes
//*****************************************************************************

#define _GNU_SOURCE

#include <dlfcn.h>



//*****************************************************************************
// libmonitor
//*****************************************************************************

#include "../libmonitor/monitor.h"



//*****************************************************************************
// local includes
//*****************************************************************************

#include "../../common/lean/spinlock.h"

#include "../trace.h"
#include "../write_data.h"

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
