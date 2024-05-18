// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

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
