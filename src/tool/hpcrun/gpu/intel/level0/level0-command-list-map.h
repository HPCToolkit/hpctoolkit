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


#ifndef level0_commandlist_map_h
#define level0_commandlist_map_h

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-api.h"
#include "level0-handle-map.h"
#include "level0-data-node.h"

//*****************************************************************************
// interface operations
//*****************************************************************************

level0_data_node_t**
level0_commandlist_map_lookup
(
 ze_command_list_handle_t command_list_handle
);

void
level0_commandlist_map_insert
(
 ze_command_list_handle_t command_list_handle
);

void
level0_commandlist_map_delete
(
 ze_command_list_handle_t command_list_handle
);

level0_data_node_t*
level0_commandlist_alloc_kernel
(
 ze_kernel_handle_t kernel,
 ze_event_handle_t event,
 ze_event_pool_handle_t event_pool
);

level0_data_node_t*
level0_commandlist_alloc_memcpy
(
 ze_memory_type_t src_type,
 ze_memory_type_t dst_type,
 size_t copy_size,
 ze_event_handle_t event,
 ze_event_pool_handle_t event_pool
);

level0_data_node_t*
level0_commandlist_append_kernel
(
 level0_data_node_t** command_list,
 ze_kernel_handle_t kernel,
 ze_event_handle_t event,
 ze_event_pool_handle_t event_pool
);

level0_data_node_t*
level0_commandlist_append_memcpy
(
 level0_data_node_t** command_list,
 ze_memory_type_t src_type,
 ze_memory_type_t dst_type,
 size_t copy_size,
 ze_event_handle_t event,
 ze_event_pool_handle_t event_pool
);
#endif