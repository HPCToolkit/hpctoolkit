// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <stdatomic.h>

#include "collections-util.h"


/* Check if CONCURRENT_ID_MAP_PREFIX is defined */
#ifdef CONCURRENT_ID_MAP_PREFIX
  #define CONCURRENT_ID_MAP_MEMBER(NAME) JOIN2(CONCURRENT_ID_MAP_PREFIX, NAME)
#else
  #define CONCURRENT_ID_MAP_MEMBER(NAME) JOIN2(id_map, NAME)
#endif


#if !defined(CONCURRENT_ID_MAP_DECLARE) && !defined(CONCURRENT_ID_MAP_DEFINE)
  #define CONCURRENT_ID_MAP_DEFINE_INPLACE
#endif


#if !defined(CONCURRENT_ID_MAP_ENTRY_INITIALIZER)
  #define CONCURRENT_ID_MAP_ENTRY_INITIALIZER {}
#endif


#if !defined(CONCURRENT_ID_MAP_BLOCK_SIZE)
  #define CONCURRENT_ID_MAP_BLOCK_SIZE 2048
#endif


#if !defined(CONCURRENT_ID_MAP_NUM_BLOCKS)
  #define CONCURRENT_ID_MAP_NUM_BLOCKS 2048
#endif


#if defined(CONCURRENT_ID_MAP_DECLARE) || defined(CONCURRENT_ID_MAP_DEFINE_INPLACE)
  #if !defined(CONCURRENT_ID_MAP_ENTRY_TYPE)
    #error CONCURRENT_ID_MAP_ENTRY_TYPE is not defined
  #endif

  #define CONCURRENT_ID_MAP_TYPE CONCURRENT_ID_MAP_MEMBER(t)
  typedef struct CONCURRENT_ID_MAP_TYPE {
    CONCURRENT_ID_MAP_ENTRY_TYPE block0[CONCURRENT_ID_MAP_BLOCK_SIZE];
    _Atomic(CONCURRENT_ID_MAP_ENTRY_TYPE*) blocks[CONCURRENT_ID_MAP_NUM_BLOCKS];

    void *(*alloc_fn)(size_t);
  } CONCURRENT_ID_MAP_TYPE;

  #define CONCURRENT_ID_MAP_INITIALIZER(ALLOC_FN)                                 \
    {                                                                             \
      .block0[0 ... (CONCURRENT_ID_MAP_BLOCK_SIZE - 1)] =                         \
        CONCURRENT_ID_MAP_ENTRY_INITIALIZER,                                      \
      .blocks[0 ... (CONCURRENT_ID_MAP_NUM_BLOCKS - 1)] = ATOMIC_VAR_INIT(NULL),  \
      .alloc_fn = ALLOC_FN                                                        \
     }

#endif


#if defined(CONCURRENT_ID_MAP_DECLARE)

void
CONCURRENT_ID_MAP_MEMBER(init)
(
 CONCURRENT_ID_MAP_TYPE *map,
 void *(*alloc_fn)(size_t)
);


CONCURRENT_ID_MAP_ENTRY_TYPE *
CONCURRENT_ID_MAP_MEMBER(get)
(
 CONCURRENT_ID_MAP_TYPE *map,
 uint32_t idx
);

#endif


#if defined (CONCURRENT_ID_MAP_DEFINE) || defined(CONCURRENT_ID_MAP_DEFINE_INPLACE)

#ifdef CONCURRENT_ID_MAP_INPLACE
#define CONCURRENT_ID_MAP_FN_MOD __attribute__((unused)) static
#else
#define CONCURRENT_ID_MAP_FN_MOD
#endif


void
CONCURRENT_ID_MAP_MEMBER(init)
(
 CONCURRENT_ID_MAP_TYPE *map,
 void *(*alloc_fn)(size_t)
)
{
  *map = (CONCURRENT_ID_MAP_TYPE) CONCURRENT_ID_MAP_INITIALIZER(alloc_fn);
}


CONCURRENT_ID_MAP_ENTRY_TYPE *
CONCURRENT_ID_MAP_MEMBER(get)
(
 CONCURRENT_ID_MAP_TYPE *map,
 uint32_t idx
)
{
  if (idx < CONCURRENT_ID_MAP_BLOCK_SIZE) {
    return &map->block0[idx];
  }

  idx -= CONCURRENT_ID_MAP_BLOCK_SIZE;
  uint32_t block_idx = idx / CONCURRENT_ID_MAP_BLOCK_SIZE;
  idx %= CONCURRENT_ID_MAP_BLOCK_SIZE;

  if (block_idx >= CONCURRENT_ID_MAP_NUM_BLOCKS) {
    return NULL;
  }

  CONCURRENT_ID_MAP_ENTRY_TYPE *block = atomic_load(&map->blocks[block_idx]);
  if (block != NULL) {
    return &block[idx];
  }

  // Allocate a new block and initialize its elements
  block = map->alloc_fn(CONCURRENT_ID_MAP_BLOCK_SIZE
    * sizeof(CONCURRENT_ID_MAP_ENTRY_TYPE));
  for (uint32_t i = 0; i < CONCURRENT_ID_MAP_BLOCK_SIZE; ++i) {
    block[i] =
      (CONCURRENT_ID_MAP_ENTRY_TYPE) CONCURRENT_ID_MAP_ENTRY_INITIALIZER;
  }

  // Try to set the newly allocated block
  for (uint32_t i = 0; i < CONCURRENT_ID_MAP_NUM_BLOCKS; ++i) {
    uint32_t bi = (block_idx + i) % CONCURRENT_ID_MAP_NUM_BLOCKS;

    CONCURRENT_ID_MAP_ENTRY_TYPE *expected = NULL;
    atomic_compare_exchange_strong(&map->blocks[bi], &expected, block);
    if (expected == NULL) {
      block = NULL;
      break;
    }
  }

  // TODO: free(block);

  block = atomic_load(&map->blocks[block_idx]);
  return &block[idx];
}

#endif
