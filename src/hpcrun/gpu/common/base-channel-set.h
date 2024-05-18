// * BeginRiceCopyright *****************************************************
// -*-Mode: C++;-*- // technically C99
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
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


#ifndef base_channel_set_h
#define base_channel_set_h

//******************************************************************************
// system includes
//******************************************************************************

#include <pthread.h>
#include <stdatomic.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct base_channel_set_t {
  size_t max_channels;
  atomic_size_t channels_size;

  size_t unprocessed_target_count;
  atomic_size_t uprocessed_count;
  pthread_mutex_t mtx;
  pthread_cond_t cond_var;

  size_t processed;
} base_channel_set_t;

#endif



//******************************************************************************
// interface operations
//******************************************************************************

void
base_channel_set_on_send_callback
(
 void *arg
);


void
base_channel_set_on_receive_callback
(
 void *arg
);


void *
base_channel_set_init
(
 base_channel_set_t *channel_set,
 size_t max_channels,
 size_t unprocessed_target_count
);


_Bool
base_channel_set_add
(
 base_channel_set_t *channel_set
);


void
base_channel_set_await
(
 base_channel_set_t *channel_set,
 size_t timeout_us
);


void
base_channel_set_notify
(
 base_channel_set_t *channel_set
);
