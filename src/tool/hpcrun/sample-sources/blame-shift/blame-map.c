// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: $
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
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
// directed blame shifting for locks, critical sections, ...
//

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include "blame-map.h"

#include <hpcrun/messages/messages.h>
#include <lib/prof-lean/atomic-op.h>
#include <memory/hpcrun-malloc.h>

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


union blame_entry_t {
  uint64_t all;
  blame_parts_t parts; 
};



/***************************************************************************
 * private data  
 ***************************************************************************/

static uint64_t volatile thelock;



/***************************************************************************
 * private operations
 ***************************************************************************/

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


uint64_t 
blame_map_entry(uint64_t obj, uint32_t metric_value)
{
  blame_entry_t be;
  be.parts.obj_id = blame_map_obj_id(obj);
  be.parts.blame = metric_value;
  return be.all;
}



/***************************************************************************
 * interface operations
 ***************************************************************************/

blame_entry_t*
blame_map_new(void)
{
  blame_entry_t* rv = hpcrun_malloc(N * sizeof(blame_entry_t));
  for(blame_entry_t* p = rv; p < rv + N; p++) {
    p->all = 0;
  }
  return rv;
}

void 
blame_map_init(blame_entry_t table[])
{
  int i;
  for(i = 0; i < N; i++)
    table[i].all = 0;
}


void
blame_map_add_blame(blame_entry_t table[],
		    uint64_t obj, uint32_t metric_value)
{
  uint32_t obj_id = blame_map_obj_id(obj);
  uint32_t index = blame_map_hash(obj);

  assert(index >= 0 && index < N);

  do_lock();
  for(;;) {
    blame_entry_t oldval = table[index];

    if(oldval.parts.obj_id == obj_id) {
#ifdef LOSSLESS_BLAME
      blame_entry_t newval = oldval;
      newval.parts.blame += metric_value;
      if (compare_and_swap_i64(&table[index].all, oldval.all, newval.all) 
	    == oldval.all) break;
#else
      oldval.parts.blame += metric_value;
      table[index].all = oldval.all;
      break;
#endif
    } else {
      if(oldval.parts.obj_id == 0) {
	uint64_t newval = blame_map_entry(obj, metric_value);
#ifdef LOSSLESS_BLAME
	if (compare_and_swap_i64(&table[index].all, oldval.all, newval) 
	    == oldval.all) break;
	// otherwise, try again
#else
	table[index].all = newval;
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

  assert(index >= 0 && index < N);

  do_lock();
  for(;;) {
    blame_entry_t oldval = table[index];
    if(oldval.parts.obj_id == obj_id) {
#ifdef LOSSLESS_BLAME
      if (compare_and_swap_i64(&table[index].all, oldval.all, zero) 
	  != oldval.all) continue; // try again on failure
#else
      table[index].all = 0;
#endif
      val = (uint64_t)oldval.parts.blame;
    }
    break;
  }
  do_unlock();

  return val;
}
