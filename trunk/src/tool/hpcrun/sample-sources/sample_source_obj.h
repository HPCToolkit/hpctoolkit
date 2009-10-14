// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample_source.h $
// $Id: sample_source.h 2587 2009-10-10 20:14:47Z johnmc $
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

#ifndef SAMPLE_SOURCE_H
#define SAMPLE_SOURCE_H

#include <stdbool.h>

#include "simple_oo.h"

// OO macros for sample_sources

#define METHOD_DEF(retn,name,...) retn (*name)(struct sample_source_t* self, ##__VA_ARGS__)

// abbreviation macro for common case of void methods
#define VMETHOD_DEF(name,...) METHOD_DEF(void,name, ##__VA_ARGS__)
#define METHOD_FN(n,...) n(sample_source_t *self, ##__VA_ARGS__)

#include "evlist.h"

// A sample source "state"
// UNINIT and INIT refer to the source
//  START and STOP are on a per-thread basis

typedef enum {
  UNINIT,
  INIT,
  START,
  STOP,
  HARD_STOP
} source_state_t;

typedef enum {
  SS_HARDWARE,    // use sample counters or other hardware.
                  // NOTE: *Currently limited to only 1 hardware class sample source*
  SS_SOFTWARE     // software-based, only return counts for the moment
} ss_class_t;

typedef struct sample_source_t {
  // common methods

  VMETHOD_DEF(add_event, const char* ev_str);
  VMETHOD_DEF(store_event, int event_id, long thresh);
  VMETHOD_DEF(store_metric_id, int event_idx, int metric_id);
  METHOD_DEF(char*, get_event_str);

  // specific methods

  VMETHOD_DEF(init);
  VMETHOD_DEF(start);
  VMETHOD_DEF(stop);
  VMETHOD_DEF(shutdown);
  METHOD_DEF(int, supports_event, const char* ev_str);
  VMETHOD_DEF(process_event_list, int lush_agents);
  VMETHOD_DEF(gen_event_set, int lush_agents);
  VMETHOD_DEF(display_events);

  // data
  evlist_t       evl;       // event list
  int            evset_idx; // index of sample source
  const char*    name;      // text name of sample source
  source_state_t state;     // state of sample source: limited to UNINIT or INIT
  ss_class_t     cls;       // kind of sample source: see ss_class_t typedef at top of file
  
} sample_source_t;


#endif // SAMPLE_SOURCE_H
