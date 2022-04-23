// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
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

#ifndef __directed_h__
#define __directed_h__

#include "sample-sources/blame-shift/blame-map.h"

#include "hpcrun/cct/cct.h"

#include <stdint.h>

typedef uint64_t (*directed_blame_target_fn)(void);

typedef struct directed_blame_info_t {
  directed_blame_target_fn get_blame_target;
  blame_entry_t* blame_table;

  int wait_metric_id;
  int blame_metric_id;

  int levels_to_skip;
} directed_blame_info_t;

void directed_blame_shift_start(void* arg, uint64_t obj);

void directed_blame_shift_end(void* arg);

void directed_blame_sample(void* arg, int metric_id, cct_node_t* node, int metric_incr);

void directed_blame_accept(void* arg, uint64_t obj);

#endif  // __directed_h__
