// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
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

#include <assert.h>
#include <string.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-handle-map.h"
#include "level0-event-map.h"
#include "lib/prof-lean/spinlock.h"

//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "hpcrun/gpu/gpu-print.h"

//******************************************************************************
// local data
//******************************************************************************

static level0_handle_map_entry_t *event_map_root = NULL;

static level0_handle_map_entry_t *event_free_list = NULL;

static spinlock_t event_map_lock = SPINLOCK_UNLOCKED;

//*****************************************************************************
// interface operations
//*****************************************************************************

level0_data_node_t*
level0_event_map_lookup
(
 ze_event_handle_t event_handle
)
{
  spinlock_lock(&event_map_lock);

  uint64_t key = (uint64_t)event_handle;
  level0_handle_map_entry_t *result = level0_handle_map_lookup(&event_map_root, key);

  PRINT("level0 event map lookup: id=0x%lx (record %p)\n",
       key, result);

  spinlock_unlock(&event_map_lock);
  if (result == NULL) return NULL;
  return *level0_handle_map_entry_data_get(result);
}

void
level0_event_map_insert
(
 ze_event_handle_t event_handle,
 level0_data_node_t* new_data
)
{
  spinlock_lock(&event_map_lock);

  uint64_t key = (uint64_t)event_handle;
  level0_handle_map_entry_t *entry = level0_handle_map_entry_new(&event_free_list, key, new_data);

  level0_handle_map_insert(&event_map_root, entry);

  PRINT("level0 event map insert: handle=%p (entry=%p)\n",
	 command_list_handle, new_entry);

  spinlock_unlock(&event_map_lock);
}

void
level0_event_map_delete
(
 ze_event_handle_t event_handle
)
{
  spinlock_lock(&event_map_lock);

  uint64_t key = (uint64_t)event_handle;
  level0_handle_map_delete(&event_map_root, &event_free_list, key);

  spinlock_unlock(&event_map_lock);
}
