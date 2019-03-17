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
// Copyright ((c)) 2002-2019, Rice University
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

#ifndef __CUSTOM_EVENT_H__
#define __CUSTOM_EVENT_H__

#include "sample_event.h"
#include <lib/prof-lean/hpcrun-fmt.h>


// forward type declaration from perf-util.h
struct event_info_s;
struct event_thread_s;
struct perf_mmap_data_s;

// callback functions
typedef void (*register_event_t)(struct event_info_s *);
typedef void (*event_handler_t)(struct event_thread_s*, sample_val_t , struct perf_mmap_data_s* );


// --------------------------------------------------------------
// data structure for our customized event
// this type should be used only within perf module.
// --------------------------------------------------------------
typedef struct event_custom_s {
  const char *name;            // unique name of the event
  const char *desc;            // brief description of the event

  register_event_t register_fn;// function to register the event
  event_handler_t  handler_fn; // callback to be used during the sampling

  int            metric_index; // hpcrun's index metric
  metric_desc_t *metric_desc;  // pointer to predefined metric
} event_custom_t;

event_custom_t *event_custom_find(const char *name);
int event_custom_register(event_custom_t *event);
void event_custom_display(FILE *std);

#endif
