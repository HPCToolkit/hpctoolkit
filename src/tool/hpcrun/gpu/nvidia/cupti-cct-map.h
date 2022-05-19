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

#ifndef cupti_cct_map_h
#define cupti_cct_map_h

#include <hpcrun/cct/cct.h>

typedef struct cupti_cct_map_entry_s cupti_cct_map_entry_t;

cupti_cct_map_entry_t *
cupti_cct_map_lookup(cct_node_t *cct);

void
cupti_cct_map_insert(cct_node_t *cct, uint32_t range_id);

void
cupti_cct_map_clear();

uint32_t
cupti_cct_map_entry_range_id_get(cupti_cct_map_entry_t *entry);

uint64_t
cupti_cct_map_entry_count_get(cupti_cct_map_entry_t *entry);

uint64_t
cupti_cct_map_entry_sampled_count_get(cupti_cct_map_entry_t *entry);

void
cupti_cct_map_entry_range_id_update(cupti_cct_map_entry_t *entry, uint32_t range_id);

void
cupti_cct_map_entry_count_increase(cupti_cct_map_entry_t *entry, uint64_t sampled_count, uint64_t count);

void
cupti_cct_map_stats();

#endif
