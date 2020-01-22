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

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/memory/hpcrun-malloc.h>

#include "gpu-channel-item-allocator.h"



//******************************************************************************
// interface functions
//******************************************************************************

s_element_t *
channel_item_alloc_helper
(
 bichannel_t *c, 
 size_t size
)
{
  s_element_t *se = bichannel_pop(c, bichannel_direction_backward);
  if (!se) {
    bichannel_steal(c, bichannel_direction_backward);
    se = bichannel_pop(c, bichannel_direction_backward);
  }
  if (!se) {
    se = (s_element_t *) hpcrun_malloc_safe(size);
    sstack_ptr_set(&se->next, 0);
  }
  return se;
}


void
channel_item_free_helper
(
 bichannel_t *c, 
 s_element_t *se
)
{
  bichannel_push(c, bichannel_direction_backward, se);
}
