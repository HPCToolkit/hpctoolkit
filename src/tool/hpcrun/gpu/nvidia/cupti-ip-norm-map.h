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

#ifndef cupti_ip_norm_map_h
#define cupti_ip_norm_map_h


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include <hpcrun/cct/cct.h>

//*****************************************************************************
// interface operations
//*****************************************************************************

typedef struct cupti_ip_norm_map_entry_s cupti_ip_norm_map_entry_t;

typedef enum {
  CUPTI_IP_NORM_MAP_NOT_EXIST = 0,
  CUPTI_IP_NORM_MAP_EXIST = 1,
  CUPTI_IP_NORM_MAP_EXIST_OTHER_THREADS = 2,
  CUPTI_IP_NORM_MAP_DUPLICATE = 3,
  CUPTI_IP_NORM_MAP_COUNT = 4,
} cupti_ip_norm_map_ret_t;


cupti_ip_norm_map_ret_t
cupti_ip_norm_map_lookup_thread
(
 ip_normalized_t ip_norm,
 cct_node_t *cct
); 


cupti_ip_norm_map_ret_t
cupti_ip_norm_map_lookup
(
 cupti_ip_norm_map_entry_t **root,
 ip_normalized_t ip_norm,
 cct_node_t *cct
); 


void
cupti_ip_norm_map_insert_thread
(
 ip_normalized_t ip_norm,
 cct_node_t *cct,
 uint32_t range_id
);


void
cupti_ip_norm_map_insert
(
 cupti_ip_norm_map_entry_t **root,
 ip_normalized_t ip_norm,
 cct_node_t *cct,
 uint32_t range_id
);


void
cupti_ip_norm_map_delete_thread
(
 ip_normalized_t ip_norm
);


void
cupti_ip_norm_map_delete
(
 cupti_ip_norm_map_entry_t **root,
 ip_normalized_t ip_norm
);


void
cupti_ip_norm_map_clear
(
 cupti_ip_norm_map_entry_t **root
);


void
cupti_ip_norm_map_clear_thread
(
);


void
cupti_ip_norm_global_map_insert
(
 ip_normalized_t ip_norm,
 cct_node_t *cct,
 uint32_t range_id
);


void
cupti_ip_norm_global_map_delete
(
 ip_normalized_t ip_norm,
 cct_node_t *cct
);


cupti_ip_norm_map_entry_t *
cupti_ip_norm_global_map_retrieve
(
 ip_normalized_t ip_norm
);


cupti_ip_norm_map_ret_t
cupti_ip_norm_global_map_lookup
(
 ip_normalized_t ip_norm,
 cct_node_t *cct
);


void
cupti_ip_norm_global_map_clear
(
);


void
cupti_ip_norm_global_map_reset
(
);


uint32_t
cupti_ip_norm_global_map_entry_range_id_get
(
 cupti_ip_norm_map_entry_t *entry
);


cupti_ip_norm_map_entry_t *
cupti_ip_norm_global_map_root_get
(
);


#endif
