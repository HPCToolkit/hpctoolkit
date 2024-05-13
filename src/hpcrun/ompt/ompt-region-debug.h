// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __OMPT_REGION_DEBUG_H__
#define __OMPT_REGION_DEBUG_H__

//*****************************************************************************
// macros
//*****************************************************************************

#define REGION_DEBUG 0

//*****************************************************************************
// macros
//*****************************************************************************

#if REGION_DEBUG == 0

// debugging disabled: empty macros replace the API

#define ompt_region_debug_notify_needed(notification)
#define ompt_region_debug_notify_received(notification)
#define ompt_region_debug_init()
#define ompt_region_debug_region_create(region)
#define hpcrun_ompt_region_check() (0)

#else



//*****************************************************************************
// local includes
//*****************************************************************************

#include "ompt-region.h"
#include "ompt-types.h"



//*****************************************************************************
// interface operations
//*****************************************************************************

// this thread requires an asynchronous notification for this region
void
ompt_region_debug_notify_needed
(
 ompt_notification_t *notification
);


// this thread received an asynchronous notification for this region
void
ompt_region_debug_notify_received
(
 ompt_notification_t *notification
);


// initialize persistent state maintained by this module
void
ompt_region_debug_init
(
 void
);


void
ompt_region_debug_region_create
(
  ompt_region_data_t* region
);


int
hpcrun_ompt_region_check
(
  void
);


#endif

#endif
