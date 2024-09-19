// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <string.h>


//******************************************************************************
// local includes
//******************************************************************************

#include "../common/gpu-channel-item-allocator.h"
#include "../../../memory/hpcrun-malloc.h"

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

opencl_object_t *
opencl_malloc
(
 void
)
{
  opencl_object_channel_t *c = opencl_object_channel_get();
  opencl_object_t *cl_obj = channel_item_alloc(c, opencl_object_t);
  memset(cl_obj, 0, sizeof(opencl_object_t));
  cl_obj->channel = c;
  return cl_obj;
}


opencl_object_t *
opencl_malloc_kind
(
  gpu_activity_kind_t kind,
  const struct hpcrun_foil_appdispatch_opencl* dispatch
)
{
  opencl_object_t *cl_obj = opencl_malloc();
  cl_obj->dispatch = dispatch;
  cl_obj->kind = kind;
  return cl_obj;
}


void
opencl_free
(
 opencl_object_t *obj
)
{
  opencl_object_channel_t *c = obj->channel;
  channel_item_free(c, obj);
}
