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
// Copyright ((c)) 2002-2019, Rice University
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

//***************************************************************************
//
// File:
//   ompt-device-map.h
//
// Purpose:
//   interface for map from device id to device data structure
//  
//***************************************************************************


#ifndef _hpctoolkit_ompt_device_map_h_
#define _hpctoolkit_ompt_device_map_h_

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>
#include <omp-tools.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <hpcrun/cct/cct.h>



//*****************************************************************************
// type definitions 
//*****************************************************************************

typedef struct ompt_device_map_entry_s ompt_device_map_entry_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

ompt_device_map_entry_t *
ompt_device_map_lookup
(
 uint64_t id
);


void 
ompt_device_map_insert
(
 uint64_t device_id, 
 ompt_device_t *ompt_device, 
 const char *type
);


bool 
ompt_device_map_refcnt_update
(
 uint64_t device_id, 
 int val
);


uint64_t 
ompt_device_map_entry_refcnt_get
(
 ompt_device_map_entry_t *entry
);


ompt_device_t *
ompt_device_map_entry_device_get
(
 ompt_device_map_entry_t *entry
);

#endif
