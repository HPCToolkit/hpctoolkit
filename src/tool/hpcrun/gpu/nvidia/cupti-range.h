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

#ifndef cupti_range_h
#define cupti_range_h

#include <stdint.h>
#include <stdbool.h>

#define CUPTI_RANGE_DEFAULT_INTERVAL 1
#define CUPTI_RANGE_DEFAULT_INTERVAL_STR "1"
#define CUPTI_RANGE_DEFAULT_SAMPLING_PERIOD 1
#define CUPTI_RANGE_DEFAULT_SAMPLING_PERIOD_STR "1"


typedef enum cupti_range_mode {
  CUPTI_RANGE_MODE_NONE = 0,
  CUPTI_RANGE_MODE_SERIAL = 1,
  CUPTI_RANGE_MODE_EVEN = 2,
  CUPTI_RANGE_MODE_TRIE = 3,
  CUPTI_RANGE_MODE_CONTEXT_SENSITIVE = 4,
  CUPTI_RANGE_MODE_COUNT = 5
} cupti_range_mode_t;


void
cupti_range_config
(
 const char *mode_str,
 int interval,
 int sampling_period,
 bool dynamic_period
);


cupti_range_mode_t
cupti_range_mode_get
(
 void
);


uint32_t
cupti_range_interval_get
(
 void
);


uint32_t
cupti_range_sampling_period_get
(
 void
);


// Called at thread finish callback to handle edge cases for the thread
void
cupti_range_thread_last
(
 void
);


// Called at process finish callback, context destroy, or module unload callback to get back pc samples for the last range
void
cupti_range_last
(
 void
);

#endif
