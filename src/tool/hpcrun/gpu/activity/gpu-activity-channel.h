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

#ifndef gpu_activity_channel_h
#define gpu_activity_channel_h

//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-activity.h"



//******************************************************************************
// forward type declarations
//******************************************************************************

typedef struct gpu_activity_channel_t gpu_activity_channel_t;



//******************************************************************************
// interface operations
//******************************************************************************

/**
 * Generate a correlation ID associated with the thread-local channel
 */
uint64_t
gpu_activity_channel_generate_correlation_id
(
 void
);


/**
 * Extract thread ID from correlation ID
 */
uint32_t
gpu_activity_channel_correlation_id_get_thread_id
(
 uint64_t correlation_id
);


/**
 * Returns the thread-local channel. If the channel is not initialized,
 * it first initializes thread-local channel
 */
gpu_activity_channel_t *
gpu_activity_channel_get_local
(
 void
);


/**
 * Returns channel whose ID is \p id. If channel is not found,
 * returns NULL.
 * The functions is thread safe.
 */
gpu_activity_channel_t *
gpu_activity_channel_lookup
(
 uint32_t thread_id
);


/**
 * Sends \p activity to \p channel
 */
void
gpu_activity_channel_send
(
 gpu_activity_channel_t *channel,
 const gpu_activity_t *activity
);


/**
 * Calls \p receive_fn for each activity in the thread-local channel;
 * each activity is passed as the argument of \p receive_fn() .
 * All activities passed to \p receive_fn are removed from the channel.
 */
void
gpu_activity_channel_receive_all
(
 void (*receive_fn)(gpu_activity_t *)
);


#endif
