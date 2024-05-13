// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   module-ignore-map.h
//
// Purpose:
//   interface definitions for a map of load modules that should be omitted
//   from call paths for synchronous samples
//
//
//***************************************************************************

#ifndef _HPCTOOLKIT_MODULE_IGNORE_MAP_H_
#define _HPCTOOLKIT_MODULE_IGNORE_MAP_H_

#include <stdbool.h>
#include "loadmap.h"

void
module_ignore_map_init
(
 void
);

bool
module_ignore_map_module_id_lookup
(
 uint16_t module_id
);


bool
module_ignore_map_module_lookup
(
 load_module_t *module
);


bool
module_ignore_map_inrange_lookup
(
 void *addr
);


bool
module_ignore_map_lookup
(
 void *start,
 void *end
);


bool
module_ignore_map_id_lookup
(
 uint16_t module_id
);


bool
module_ignore_map_ignore
(
 load_module_t* lm
);


bool
module_ignore_map_delete
(
 load_module_t* lm
);


#endif
