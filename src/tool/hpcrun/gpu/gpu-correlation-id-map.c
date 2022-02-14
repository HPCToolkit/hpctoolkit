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
// Copyright ((c)) 2002-2022, Rice University
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

#include <lib/prof-lean/splay-uint64.h>

#include "gpu-correlation-id-map.h"
#include "gpu-splay-allocator.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG 0

#include "gpu-print.h"


#define st_insert				\
  typed_splay_insert(correlation_id)

#define st_lookup				\
  typed_splay_lookup(correlation_id)

#define st_delete				\
  typed_splay_delete(correlation_id)

#define st_forall				\
  typed_splay_forall(correlation_id)

#define st_count				\
  typed_splay_count(correlation_id)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, gpu_correlation_id_map_entry_t)

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)



//*****************************************************************************
// type declarations
//*****************************************************************************

#undef typed_splay_node
#define typed_splay_node(correlation_id) gpu_correlation_id_map_entry_t

typedef struct typed_splay_node(correlation_id) {
  struct typed_splay_node(correlation_id) *left;
  struct typed_splay_node(correlation_id) *right;
  uint64_t gpu_correlation_id; // key

  uint64_t host_correlation_id;
  uint32_t device_id;
  uint64_t start;
  uint64_t end;
} typed_splay_node(correlation_id);





//******************************************************************************
// local data
//******************************************************************************

static __thread gpu_correlation_id_map_entry_t *map_root = NULL;

static __thread gpu_correlation_id_map_entry_t *free_list = NULL;



//*****************************************************************************
// private operations
//*****************************************************************************

typed_splay_impl(correlation_id)


static gpu_correlation_id_map_entry_t *
gpu_correlation_id_map_entry_alloc()
{
  return st_alloc(&free_list);
}


static gpu_correlation_id_map_entry_t *
gpu_correlation_id_map_entry_new
(
 uint64_t gpu_correlation_id,
 uint64_t host_correlation_id
)
{
  gpu_correlation_id_map_entry_t *e = gpu_correlation_id_map_entry_alloc();

  memset(e, 0, sizeof(gpu_correlation_id_map_entry_t));

  e->gpu_correlation_id = gpu_correlation_id;
  e->host_correlation_id = host_correlation_id;

  return e;
}



//*****************************************************************************
// interface operations
//*****************************************************************************

gpu_correlation_id_map_entry_t *
gpu_correlation_id_map_lookup
(
 uint64_t gpu_correlation_id
)
{
  uint64_t correlation_id = gpu_correlation_id;
  gpu_correlation_id_map_entry_t *result = st_lookup(&map_root, correlation_id);

  PRINT("correlation_id map lookup: id=0x%lx (record %p)\n",
       correlation_id, result);

  return result;
}


void
gpu_correlation_id_map_insert
(
 uint64_t gpu_correlation_id,
 uint64_t host_correlation_id
)
{
  if (st_lookup(&map_root, gpu_correlation_id)) {
    // fatal error: correlation_id already present; a
    // correlation should be inserted only once.
    assert(0);
  } else {
    gpu_correlation_id_map_entry_t *entry =
      gpu_correlation_id_map_entry_new(gpu_correlation_id, host_correlation_id);

    st_insert(&map_root, entry);

    PRINT("correlation_id_map insert: correlation_id=0x%lx external_id=%ld (entry=%p)\n",
	  gpu_correlation_id, host_correlation_id, entry);
  }
}


// TODO(Keren): remove
void
gpu_correlation_id_map_external_id_replace
(
 uint64_t gpu_correlation_id,
 uint64_t host_correlation_id
)
{
  PRINT("correlation_id map replace: id=0x%x\n", gpu_correlation_id);

  gpu_correlation_id_map_entry_t *entry = st_lookup(&map_root, gpu_correlation_id);
  if (entry) {
    entry->host_correlation_id = host_correlation_id;
  }
}


void
gpu_correlation_id_map_delete
(
 uint64_t gpu_correlation_id
)
{
  gpu_correlation_id_map_entry_t *node = st_delete(&map_root, gpu_correlation_id);
  st_free(&free_list, node);
}


void
gpu_correlation_id_map_kernel_update
(
 uint64_t gpu_correlation_id,
 uint32_t device_id,
 uint64_t start,
 uint64_t end
)
{
  uint64_t correlation_id = gpu_correlation_id;
  PRINT("correlation_id map replace: id=0x%lx\n", correlation_id);

  gpu_correlation_id_map_entry_t *entry = st_lookup(&map_root, correlation_id);
  if (entry) {
    entry->device_id = device_id;
    entry->start = start;
    entry->end = end;
  }
}


uint64_t
gpu_correlation_id_map_entry_external_id_get
(
 gpu_correlation_id_map_entry_t *entry
)
{
  return entry->host_correlation_id;
}


uint64_t
gpu_correlation_id_map_entry_start_get
(
 gpu_correlation_id_map_entry_t *entry
)
{
  return entry->start;
}


uint64_t
gpu_correlation_id_map_entry_end_get
(
 gpu_correlation_id_map_entry_t *entry
)
{
  return entry->end;
}


uint64_t
gpu_correlation_id_map_entry_device_id_get
(
 gpu_correlation_id_map_entry_t *entry
)
{
  return entry->device_id;
}



//*****************************************************************************
// debugging code
//*****************************************************************************

uint64_t
gpu_correlation_id_map_count
(
 void
)
{
  return st_count(map_root);
}
