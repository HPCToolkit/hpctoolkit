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
