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

#ifndef cupti_cct_trie_h
#define cupti_cct_trie_h

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include <hpcrun/cct/cct.h>

#define CUPTI_CCT_TRIE_UNWIND_ROOT 0
#define CUPTI_CCT_TRIE_COMPRESS_THRESHOLD 100000
#define CUPTI_CCT_TRIE_COMPRESS_THRESHOLD_STR "100000"
#define CUPTI_CCT_TRIE_PTR_NULL (CUPTI_CCT_TRIE_COMPRESS_THRESHOLD + 1)

typedef struct cupti_cct_trie_node_s cupti_cct_trie_node_t;

// Append a cct to the trie.
// Return true if a new range is encountered
bool
cupti_cct_trie_append
(
 uint32_t range_id,
 cct_node_t *cct
);

// The previous ccts from cur to logic_root are marked as a range.
// Return either the range_id at the end node or 
// GPU_RANGE_NULL to indicate this is a new range
uint32_t
cupti_cct_trie_flush
(
 uint32_t context_id,
 bool active,
 bool logic
);

// Return the trie root of the querying thread
cupti_cct_trie_node_t *
cupti_cct_trie_root_get
(
);

// Process pending notifications
void
cupti_cct_trie_notification_process
(
);

// Unwind the trie for one layer
void
cupti_cct_trie_unwind
(
);

// Clean all cct nodes
void
cupti_cct_trie_cleanup
(
);

// Dump cct trie statistics
void
cupti_cct_trie_dump
(
);

// Compress single path
void
cupti_cct_trie_compress
(
);

// Get the number of contiguous parent nodes with a single path
uint32_t
cupti_cct_trie_single_path_length_get
(
);

#endif
