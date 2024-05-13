// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
// directed blame shifting for locks, critical sections, ...
//

/******************************************************************************
 * system includes
 *****************************************************************************/




/******************************************************************************
 * local includes
 *****************************************************************************/

#define _GNU_SOURCE

#include "blame-map.h"

#include "../../messages/messages.h"
#include <stdatomic.h>
#include "../../memory/hpcrun-malloc.h"

/******************************************************************************
 * macros
 *****************************************************************************/

#define N (128*1024)
#define INDEX_MASK ((N)-1)

#define LOSSLESS_BLAME

#ifdef BLAME_MAP_LOCKING

#define do_lock() \
{ \
  for(;;) { \
    if (fetch_and_store_i64(&thelock, 1) == 0) break; \
    while (thelock == 1); \
  } \
}

#define do_unlock() thelock = 0

#else

#define do_lock()
#define do_unlock()

#endif



/******************************************************************************
 * data type
 *****************************************************************************/

typedef struct {
  uint32_t obj_id;
  uint32_t blame;
} blame_parts_t;

typedef union {
  uint_fast64_t combined;
  blame_parts_t parts;
} blame_all_t;


union blame_entry_t {
  atomic_uint_fast64_t value;
};



/***************************************************************************
 * private data
 ***************************************************************************/

__attribute__((unused))
static uint64_t volatile thelock;



/***************************************************************************
 * private operations
 ***************************************************************************/

uint32_t
blame_entry_obj_id(uint_fast64_t value)
{
  blame_all_t entry;
  entry.combined = value;
  return entry.parts.obj_id;
}


uint32_t
blame_entry_blame(uint_fast64_t value)
{
  blame_all_t entry;
  entry.combined = value;
  return entry.parts.blame;
}

uint32_t
blame_map_obj_id(uint64_t obj)
{
  return ((uint32_t) ((uint64_t)obj) >> 2);
}


uint32_t
blame_map_hash(uint64_t obj)
{
  return ((uint32_t)((blame_map_obj_id(obj)) & INDEX_MASK));
}


uint_fast64_t
blame_map_entry(uint64_t obj, uint32_t metric_value)
{
  blame_all_t be;
  be.parts.obj_id = blame_map_obj_id(obj);
  be.parts.blame = metric_value;
  return be.combined;
}



/***************************************************************************
 * interface operations
 ***************************************************************************/

blame_entry_t*
blame_map_new(void)
{
  blame_entry_t* rv = hpcrun_malloc(N * sizeof(blame_entry_t));
  blame_map_init(rv);
  return rv;
}

void
blame_map_init(blame_entry_t table[])
{
  int i;
  for(i = 0; i < N; i++) {
    atomic_store(&table[i].value, 0);
  }
}


void
blame_map_add_blame(blame_entry_t table[],
                    uint64_t obj, uint32_t metric_value)
{
  uint32_t obj_id = blame_map_obj_id(obj);
  uint32_t index = blame_map_hash(obj);

  if (index >= N)
    hpcrun_terminate();

  do_lock();
  uint_fast64_t oldval = atomic_load(&table[index].value);

  for(;;) {
    blame_all_t newval;
    newval.combined = oldval;
    newval.parts.blame += metric_value;

    uint32_t obj_at_index = newval.parts.obj_id;
    if(obj_at_index == obj_id) {
#ifdef LOSSLESS_BLAME
      if (atomic_compare_exchange_strong_explicit(&table[index].value, &oldval, newval.combined,
                                                  memory_order_relaxed, memory_order_relaxed))
        break;
#else
      // the atomicity is not needed here, but it is the easiest way to write this
      atomic_store(&table[index].value, newval.combined);
      break;
#endif
    } else {
      if(newval.parts.obj_id == 0) {
        newval.combined = blame_map_entry(obj, metric_value);
#ifdef LOSSLESS_BLAME
        if ((atomic_compare_exchange_strong_explicit(&table[index].value, &oldval, newval.combined,
                                                     memory_order_relaxed, memory_order_relaxed)))
          break;
        // otherwise, try again
#else
        // the atomicity is not needed here, but it is the easiest way to write this
        atomic_store(&table[index].value, newval.combined);
        break;
#endif
      }
      else {
        EMSG("leaked blame %d\n", metric_value);
        // entry in use for another object's blame
        // in this case, since it isn't easy to shift
        // our blame efficiently, we simply drop it.
        break;
      }
    }
  }
  do_unlock();
}


uint64_t
blame_map_get_blame(blame_entry_t table[], uint64_t obj)
{
  static uint64_t zero = 0;
  uint64_t val = 0;
  uint32_t obj_id = blame_map_obj_id(obj);
  uint32_t index = blame_map_hash(obj);

  if (index >= N)
    hpcrun_terminate();

  do_lock();
  for(;;) {
    uint_fast64_t oldval = atomic_load(&table[index].value);
    uint32_t entry_obj_id = blame_entry_obj_id(oldval);
    if(entry_obj_id == obj_id) {
#ifdef LOSSLESS_BLAME
      if (!atomic_compare_exchange_strong_explicit(&table[index].value, &oldval, zero,
                                                  memory_order_relaxed, memory_order_relaxed))
        continue; // try again on failure
#else
      table[index].all = 0;
#endif
      val = (uint64_t) blame_entry_blame(oldval);
    }
    break;
  }
  do_unlock();

  return val;
}
