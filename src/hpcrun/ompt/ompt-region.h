// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __OMPT_REGION_H__
#define __OMPT_REGION_H__


//*****************************************************************************
// local includes
//*****************************************************************************

#include "ompt-types.h"



//*****************************************************************************
// interface operations
//*****************************************************************************

// initialize support for regions
void
ompt_regions_init
(
 void
);


void
ompt_parallel_region_register_callbacks
(
 ompt_set_callback_t ompt_set_callback_fn
);


// free adds entity to freelist
void
hpcrun_ompt_region_free
(
 ompt_region_data_t *region_data
);



#endif
