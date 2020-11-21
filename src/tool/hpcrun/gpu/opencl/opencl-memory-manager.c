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
// system includes
//******************************************************************************

#include <string.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/gpu/gpu-channel-item-allocator.h>
#include <hpcrun/memory/hpcrun-malloc.h>

#include "opencl-memory-manager.h"



//******************************************************************************
// type declarations
//******************************************************************************

struct opencl_object_channel_t {
  bistack_t bistacks[2];
};



//******************************************************************************
// local data
//******************************************************************************

static __thread opencl_object_channel_t *opencl_object_channel;



//******************************************************************************
// private operations
//******************************************************************************

static opencl_object_channel_t *
opencl_object_channel_alloc
(
 void
)
{
  return hpcrun_malloc_safe(sizeof(opencl_object_channel_t));
}


static opencl_object_channel_t *
opencl_object_channel_get
(
 void
)
{
  if (opencl_object_channel == NULL) {
    opencl_object_channel = opencl_object_channel_alloc();
  }
  return opencl_object_channel;
}



//******************************************************************************
// interface operations
//******************************************************************************

opencl_object_t*
opencl_malloc
(
  void
)
{
  opencl_object_channel_t *c = opencl_object_channel_get();
  opencl_object_t *cl_obj = channel_item_alloc(c, opencl_object_t);
  cl_obj->channel = c;
  return cl_obj;
}


void
opencl_free
(
  opencl_object_t *o
)
{
  memset(o, 0, sizeof(opencl_object_t));
  opencl_object_channel_t *c = &(o->channel);
  channel_item_free(c, o);
}
