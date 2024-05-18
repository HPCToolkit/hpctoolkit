// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   cuda-device-map.h
//
// Purpose:
//   interface definitions for map that enables one to look up device
//   properties given a device id
//
//***************************************************************************

#ifndef cuda_device_map_h
#define cuda_device_map_h



//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include "cuda-api.h"



//*****************************************************************************
// type definitions
//*****************************************************************************

typedef struct cuda_device_map_entry_t cuda_device_map_entry_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

cuda_device_map_entry_t *
cuda_device_map_lookup
(
 uint32_t id
);


void
cuda_device_map_insert
(
 uint32_t device
);


void
cuda_device_map_delete
(
 uint32_t device
);


cuda_device_property_t *
cuda_device_map_entry_device_property_get
(
 cuda_device_map_entry_t *entry
);



#endif
