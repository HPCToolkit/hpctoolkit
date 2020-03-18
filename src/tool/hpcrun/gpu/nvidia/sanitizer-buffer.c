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

//******************************************************************************
// macros
//******************************************************************************

#define UNIT_TEST 0

#define DEBUG 1

#include "../gpu-print.h"

//******************************************************************************
// local includes
//******************************************************************************

#include "sanitizer-buffer.h"
#include "sanitizer-api.h"

#include <stddef.h>
#include <gpu-patch.h>
#include <redshow.h>

#include <hpcrun/memory/hpcrun-malloc.h>

#include "sanitizer-buffer-channel.h"
#include "../gpu-channel-item-allocator.h"


//******************************************************************************
// type declarations
//******************************************************************************

typedef struct sanitizer_buffer_t {
  s_element_t next;

  uint32_t thread_id;
  uint32_t cubin_id;
  uint64_t kernel_id;
  uint64_t host_op_id;
  gpu_patch_buffer_t *gpu_patch_buffer;
} sanitizer_buffer_t;

//******************************************************************************
// interface operations 
//******************************************************************************

void
sanitizer_buffer_process
(
 sanitizer_buffer_t *b
)
{
  uint32_t thread_id = b->thread_id;
  uint32_t cubin_id = b->cubin_id;
  uint64_t kernel_id = b->kernel_id;
  uint64_t host_op_id = b->host_op_id;

  gpu_patch_buffer_t *gpu_patch_buffer = b->gpu_patch_buffer;
  
  redshow_analyze(thread_id, cubin_id, kernel_id, host_op_id, gpu_patch_buffer);
}


sanitizer_buffer_t *
sanitizer_buffer_alloc
(
 sanitizer_buffer_channel_t *channel
)
{
  return channel_item_alloc(channel, sanitizer_buffer_t);
}


void
sanitizer_buffer_produce
(
 sanitizer_buffer_t *b,
 uint32_t thread_id,
 uint32_t cubin_id,
 uint64_t kernel_id,
 uint64_t host_op_id,
 size_t num_records,
 atomic_uint *balance
)
{
  b->thread_id = thread_id;
  b->cubin_id = cubin_id;
  b->kernel_id = kernel_id;
  b->host_op_id = host_op_id;

  // Increase balance
  atomic_fetch_add(balance, 1);
  if (b->gpu_patch_buffer == NULL) {
    // Spin waiting
    while (atomic_load(balance) >= sanitizer_buffer_pool_size_get()) {
      sanitizer_process_signal();
    }
    PRINT("Allocate buffer size %lu\n", num_records * sizeof(gpu_patch_record_t));
    b->gpu_patch_buffer = (gpu_patch_buffer_t *) hpcrun_malloc_safe(sizeof(gpu_patch_buffer_t));
    b->gpu_patch_buffer->records = (gpu_patch_record_t *) hpcrun_malloc_safe(
      num_records * sizeof(gpu_patch_record_t));
  } else {
    PRINT("Reuse buffer size %lu\n", num_records * sizeof(gpu_patch_record_t));
  }
}


void
sanitizer_buffer_free
(
 sanitizer_buffer_channel_t *channel, 
 sanitizer_buffer_t *b,
 atomic_uint *balance
)
{
  channel_item_free(channel, b);
  atomic_fetch_add(balance, -1);
}


gpu_patch_buffer_t *
sanitizer_buffer_entry_gpu_patch_buffer_get
(
 sanitizer_buffer_t *b
)
{
  return b->gpu_patch_buffer;
}
