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
// Copyright ((c)) 2002-2024, Rice University
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

#define _GNU_SOURCE

#include <stdlib.h>
#include <limits.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "../../../../messages/messages.h"

#include "../../../../../../lib/prof-lean/crypto-hash.h"
#include "../../../../../../lib/prof-lean/spinlock.h"

#include "level0-binary.h"
#include "level0-handle-map.h"


//*****************************************************************************
// type declarations
//*****************************************************************************

typedef struct {
  const char *hash_string;
  gpu_binary_kind_t bkind;
} module_info_t;



//******************************************************************************
// local variables
//******************************************************************************

static level0_handle_map_entry_t *module_map_root = NULL;

static level0_handle_map_entry_t *module_free_list = NULL;

static spinlock_t module_lock = SPINLOCK_UNLOCKED;



//******************************************************************************
// private operations
//******************************************************************************

module_info_t *
module_info_new
(
  const char *hash_string,
  gpu_binary_kind_t bkind
)
{
  module_info_t *mi = (module_info_t *) malloc(sizeof(module_info_t));

  mi->hash_string = hash_string;
  mi->bkind = bkind;

  return mi;
}


void
level0_module_handle_map_insert
(
  ze_module_handle_t module,
  const char* hash_string,
  gpu_binary_kind_t bkind
)
{
  spinlock_lock(&module_lock);

  uint64_t key = (uint64_t)module;
  level0_handle_map_entry_t *entry =
    level0_handle_map_entry_new(&module_free_list, key, (level0_data_node_t*) module_info_new(hash_string, bkind));
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

  gpu_binary_kind_t bkind = gpu_binary_kind((const char *) buf, size);

  switch (bkind){
  case gpu_binary_kind_intel_patch_token:
    TMSG(LEVEL0, "INFO: hpcrun Level Zero binary kind: Intel Patch Token");
    break;
  case gpu_binary_kind_elf:
    TMSG(LEVEL0, "INFO: hpcrun Level Zero binary kind: ELF");
    break;
  case gpu_binary_kind_empty:
    TMSG(LEVEL0, "WARNING: hpcrun: Level Zero presented an empty GPU binary.\n"
         "Instruction-level may not be possible for kernels in this binary");
    break;
  case gpu_binary_kind_unknown:
    {
    const char *magic = (const char *) buf;
    TMSG(LEVEL0, "WARNING: hpcrun: Level Zero presented unknown binary kind: magic number='%c%c%c%c'\n"
         "Instruction-level may not be possible for kernels in this binary",
          magic[0], magic[1], magic[2], magic[3]);
    }
    break;
  case gpu_binary_kind_malformed:
    TMSG(LEVEL0, "WARNING: hpcrun: Level Zero presented a malformed GPU binary.\n"
         "Instruction-level may not be possible for kernels in this binary");
    break;
  }

  level0_module_handle_map_insert(module, hash_buf, bkind);
}


void
level0_module_handle_map_lookup
(
  ze_module_handle_t module,
  const char **hash_string,
  gpu_binary_kind_t *bkind
)
{
  spinlock_lock(&module_lock);

  uint64_t key = (uint64_t)module;
  level0_handle_map_entry_t *entry =
    level0_handle_map_lookup(&module_map_root, key);
  module_info_t  *mi = (module_info_t *) (*level0_handle_map_entry_data_get(entry));
  spinlock_unlock(&module_lock);

  *hash_string = mi->hash_string;
  *bkind = mi->bkind;
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
