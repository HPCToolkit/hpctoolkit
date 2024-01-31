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

#ifndef rocprofiler_api_h
#define rocprofiler_api_h

//******************************************************************************
// global includes
//******************************************************************************

#include <stdint.h>


//******************************************************************************
// macro definitions
//******************************************************************************

#define ROCTRACER_CHANNEL_IDX 0
#define ROCPROFILER_CHANNEL_IDX 1

//******************************************************************************
// interface operations
//******************************************************************************

void
rocprofiler_start_kernel
(
 uint64_t correlation_id
);


void
rocprofiler_stop_kernel
(
 void
);


void
rocprofiler_init
(
 void
);


void
rocprofiler_fini
(
 void *args,
 int how
);


int
rocprofiler_bind
(
  void
);

void
rocprofiler_wait_context_callback
(
  void
);

int
rocprofiler_total_counters
(
  void
);

const char*
rocprofiler_counter_name
(
  int idx
);

const char*
rocprofiler_counter_description
(
  int idx
);

int
rocprofiler_match_event
(
  const char *ev_str
);

void
rocprofiler_finalize_event_list
(
  void
);

void
rocprofiler_uri_setup
(
  void
);

void
rocprofiler_register_counter_callbacks
(
  void
);

// NB: The argument is a pointer to a rocprofiler_settings_t
void foilbase_OnLoadToolProp(void* settings);

void foilbase_OnUnloadTool();


#endif
