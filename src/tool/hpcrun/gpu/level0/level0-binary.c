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
// Copyright ((c)) 2002-2023, Rice University
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

#include <stdlib.h>
#include <limits.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-binary.h"
#include "level0-handle-map.h"
#include "lib/prof-lean/crypto-hash.h"
#include "lib/prof-lean/spinlock.h"



//******************************************************************************
// local variables
//******************************************************************************

static level0_handle_map_entry_t *module_map_root = NULL;

static level0_handle_map_entry_t *module_free_list = NULL;

static spinlock_t module_lock = SPINLOCK_UNLOCKED;

//******************************************************************************
// private operations
//******************************************************************************

void
level0_module_handle_map_insert
(
  ze_module_handle_t module,
  char* hash_buf
)
{
  spinlock_lock(&module_lock);

  uint64_t key = (uint64_t)module;
  level0_handle_map_entry_t *entry =
    level0_handle_map_entry_new(&module_free_list, key, (level0_data_node_t*)hash_buf);
  level0_handle_map_insert(&module_map_root, entry);

  spinlock_unlock(&module_lock);
}

//******************************************************************************
// interface operations
//******************************************************************************

void
level0_binary_process
(
  ze_module_handle_t module
)
{
  // Get the debug binary
  size_t size;
  zetModuleGetDebugInfo(
    module,
    ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF,
    &size,
    NULL
  );

  uint8_t* buf = (uint8_t*) malloc(size);
  zetModuleGetDebugInfo(
    module,
    ZET_MODULE_DEBUG_INFO_FORMAT_ELF_DWARF,
    &size,
    buf
  );

  // Generate a hash for the binary
  char *hash_buf = (char *) malloc(CRYPTO_HASH_STRING_LENGTH);
  crypto_compute_hash_string(buf, size, hash_buf, CRYPTO_HASH_STRING_LENGTH);

  level0_module_handle_map_insert(module, hash_buf);
}

char*
level0_module_handle_map_lookup
(
  ze_module_handle_t module
)
{
  spinlock_lock(&module_lock);

  uint64_t key = (uint64_t)module;
  level0_handle_map_entry_t *entry =
    level0_handle_map_lookup(&module_map_root, key);
  char *result = (char*) (*level0_handle_map_entry_data_get(entry));
  spinlock_unlock(&module_lock);
  return result;
}

void
level0_module_handle_map_delete
(
  ze_module_handle_t module
)
{
  spinlock_lock(&module_lock);

  uint64_t key = (uint64_t)module;
  level0_handle_map_delete(
    &module_map_root,
    &module_free_list,
    key
  );

  spinlock_unlock(&module_lock);
}
