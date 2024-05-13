// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

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
