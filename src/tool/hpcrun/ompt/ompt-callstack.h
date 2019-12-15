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

#ifndef __OMPT_CALLSTACK_H__
#define __OMPT_CALLSTACK_H__


//******************************************************************************
// system includes  
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// local includes  
//******************************************************************************

#include <hpcrun/cct/cct.h>

#include "omp-tools.h"



//******************************************************************************
// interface functions  
//******************************************************************************

void ompt_callstack_init
(
 void
);


cct_node_t *
ompt_region_context_eager
(
 uint64_t region_id, 
 ompt_scope_endpoint_t se_type,
 int adjust_callsite
);


cct_node_t *
ompt_parallel_begin_context
(
 ompt_id_t region_id, 
 int adjust_callsite
);


cct_node_t *
ompt_region_root
(
 cct_node_t *_node
);


void 
ompt_record_thread_type
(
 ompt_thread_t type
);


void
ompt_region_context_lazy
(
 uint64_t region_id,
 ompt_scope_endpoint_t se_type,
 int adjust_callsite
);


int
ompt_eager_context_p
(
 void
);

#endif
