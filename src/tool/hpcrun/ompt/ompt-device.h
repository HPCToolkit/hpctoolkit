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
// Copyright ((c)) 2002-2022, Rice University
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

#ifndef __OMPT_DEVICE_H__
#define __OMPT_DEVICE_H__

#include <stdbool.h>
#include <include/hpctoolkit-config.h>
#include <cct/cct.h>

void 
prepare_device
(
 void
);


//---------------------------------------------
// If a API is invoked by OMPT (TRUE/FALSE)
//---------------------------------------------

bool
ompt_runtime_status_get
(
 void
);


cct_node_t *
ompt_trace_node_get
(
 void
);


//-----------------------------------------------------------------------------
// NVIDIA GPU pc sampling support
//-----------------------------------------------------------------------------

void 
ompt_pc_sampling_enable
(
 void 
);


void 
ompt_pc_sampling_disable
(
 void
);

//-----------------------------------------------------------------------------
// Use hpctoolkit callback/OMPT callback
//-----------------------------------------------------------------------------

void
ompt_external_subscriber_enable
(
 void
);


void
ompt_external_subscriber_disable
(
 void
);

#endif // _OMPT_INTERFACE_H_
