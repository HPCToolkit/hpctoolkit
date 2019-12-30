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
// local includes
//******************************************************************************

//******************************************************************************
// local includes
//******************************************************************************


#include <lib/prof-lean/bichannel.h>

#include <hpcrun/memory/hpcrun-malloc.h>

#include "sanitizer-buffer-channel.h"
#include "sanitizer-buffer-channel-set.h"


//******************************************************************************
// macros
//******************************************************************************

#undef typed_bichannel
#undef typed_stack_elem

#define typed_bichannel(x) sanitizer_buffer_channel_t
#define typed_stack_elem(x) sanitizer_buffer_t

// define macros that simplify use of buffer channel API 
#define channel_init  \
  typed_bichannel_init(sanitizer_buffer_t)

#define channel_pop   \
  typed_bichannel_pop(sanitizer_buffer_t)

#define channel_push  \
  typed_bichannel_push(sanitizer_buffer_t)

#define channel_steal \
  typed_bichannel_steal(sanitizer_buffer_t)

//******************************************************************************
// type declarations
//******************************************************************************

typedef struct sanitizer_buffer_channel_t {
  bistack_t bistacks[2];
} sanitizer_buffer_channel_t;

//******************************************************************************
// local data
//******************************************************************************

static __thread sanitizer_buffer_channel_t *sanitizer_buffer_channel = NULL;

//******************************************************************************
// private functions
//******************************************************************************

// implement bidirectional channels for buffers
typed_bichannel_impl(sanitizer_buffer_t)

static sanitizer_buffer_channel_t *
sanitizer_buffer_channel_alloc
(
 void
)
{
  sanitizer_buffer_channel_t *b = 
    hpcrun_malloc_safe(sizeof(sanitizer_buffer_channel_t));

  channel_init(b);

  sanitizer_buffer_channel_set_insert(b);

  return b;
}


static sanitizer_buffer_channel_t *
sanitizer_buffer_channel_get
(
 void
)
{
  if (sanitizer_buffer_channel == NULL) {
    sanitizer_buffer_channel = sanitizer_buffer_channel_alloc();
  }

  return sanitizer_buffer_channel;
}


//******************************************************************************
// interface functions
//******************************************************************************

sanitizer_buffer_t *
sanitizer_buffer_channel_produce
(
 size_t num_records
)
{
  sanitizer_buffer_channel_t *buf_channel = sanitizer_buffer_channel_get();

  sanitizer_buffer_t *b = sanitizer_buffer_alloc(buf_channel);

  sanitizer_buffer_produce(b, num_records);

  return b;
}


void
sanitizer_buffer_channel_push
(
 sanitizer_buffer_t *b
)
{
  sanitizer_buffer_channel_t *buf_channel = sanitizer_buffer_channel_get();

  channel_push(buf_channel, bichannel_direction_forward, b);
}


void
sanitizer_buffer_channel_consume
(
 sanitizer_buffer_channel_t *channel
)
{
  // steal elements previously enqueued by the producer
  channel_steal(channel, bichannel_direction_forward);

  // consume all elements enqueued before this function was called
  for (;;) {
    sanitizer_buffer_t *b = channel_pop(channel, bichannel_direction_forward);
    if (!b) break;
    sanitizer_buffer_process(b);
    sanitizer_buffer_free(channel, b);
  }
}
