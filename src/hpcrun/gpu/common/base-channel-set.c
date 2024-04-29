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

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <pthread.h>
#include <time.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <stdatomic.h>

#include "base-channel-set.h"



//******************************************************************************
// macros
//******************************************************************************

#define MICROSECONDS_IN_SECOND        1000000
#define NANOSECONDS_IN_MICROSECOND    1000



//******************************************************************************
// interface operations
//******************************************************************************

void
base_channel_set_on_send_callback
(
 void *arg
)
{
  base_channel_set_t *channel_set = arg;
  size_t unprocessed_cnt
    = atomic_fetch_add(&channel_set->uprocessed_count, 1) + 1;

  if (unprocessed_cnt == channel_set->unprocessed_target_count) {
    pthread_cond_signal(&channel_set->cond_var);
  }
}


void
base_channel_set_on_receive_callback
(
 void *arg
)
{
  base_channel_set_t *channel_set = arg;
  ++channel_set->processed;
}


void *
base_channel_set_init
(
 base_channel_set_t *channel_set,
 size_t max_channels,
 size_t unprocessed_target_count
)
{
  channel_set->max_channels = max_channels;
  atomic_init(&channel_set->channels_size, 0);

  channel_set->unprocessed_target_count = unprocessed_target_count;
  atomic_init(&channel_set->uprocessed_count, 0);
  pthread_mutex_init(&channel_set->mtx, NULL);
  pthread_cond_init(&channel_set->cond_var, NULL);

  channel_set->processed = 0;

  return channel_set;
}


_Bool
base_channel_set_add
(
 base_channel_set_t *channel_set
)
{
  if (atomic_fetch_add(&channel_set->channels_size, 1) >= channel_set->max_channels) {
    /* Reverse the add. */
    atomic_fetch_add(&channel_set->channels_size, -1);
    return 0;
  }

  return 1;
}


void
base_channel_set_await
(
 base_channel_set_t *channel_set,
 size_t timeout_us
)
{
  size_t processed = channel_set->processed;
  size_t unprocessed
    = atomic_fetch_sub(&channel_set->uprocessed_count, processed) - processed;

  channel_set->processed = 0;

  if (unprocessed >= channel_set->unprocessed_target_count) {
    return;
  }

  /* Wait until signaled or timeout expiers */
  pthread_mutex_lock(&channel_set->mtx);

  if (timeout_us > 0) {
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    time.tv_nsec += (timeout_us % MICROSECONDS_IN_SECOND) * NANOSECONDS_IN_MICROSECOND;
    time.tv_sec += timeout_us / MICROSECONDS_IN_SECOND;
    pthread_cond_timedwait(&channel_set->cond_var, &channel_set->mtx, &time);
  } else {
    pthread_cond_wait(&channel_set->cond_var, &channel_set->mtx);
  }

  pthread_mutex_unlock(&channel_set->mtx);
}

void
base_channel_set_notify
(
 base_channel_set_t *channel_set
)
{
  pthread_cond_signal(&channel_set->cond_var);
}
